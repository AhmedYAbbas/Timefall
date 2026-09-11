import Common.Bindings;

// Penumbra caps. The world cap is what keeps apparent softness independent of shadow map resolution
// and of which cascade a pixel lands in; the texel cap is a sampling-quality limit, since a kernel
// wider than this undersamples at any tap count we can afford
static const float MAX_FILTER_WORLD = 0.5f;
static const float MAX_FILTER_TEXELS = 64.0f;	  // ceiling at PCF_REFERENCE_SAMPLES, scaled below to hold tap density constant
static const float PCF_REFERENCE_SAMPLES = 64.0f; // the slider's maximum
static const float MIN_FILTER_TEXELS = 1.75f;	  // below ~1.5 the taps collapse onto the hardware 2x2 and the edge aliases
static const float SEARCH_GAP_WORLD = 4.0f;	  // caster gap, in world units, the blocker search reaches for
static const float FIXED_PCF_TEXELS = 2.0f;	  // fixed-kernel radius when soft shadows are off
static const float GOLDEN_ANGLE = 2.39996323f;

static const float SPOT_SHADOW_NEAR = 0.05f; // twin of the near plane in ComputeSpotMatrix
static const float SPOT_SHADOW_MIN_FAR = 0.1f; // twin of its max(range, 0.1) far clamp

// Interleaved gradient noise (Jimenez 2014): low-discrepancy per-pixel seed
float InterleavedGradientNoise(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
}

// Second seed from the same pixel. The basis is swapped rather than offset: an offset would only
// translate the first field, leaving the two correlated
float InterleavedGradientNoiseAlt(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.00583715f, 0.06711056f))));
}

// Evenly-distributed disk sample (Vogel/sunflower), rotated per pixel
float2 VogelDisk(float index, float count, float rotation)
{
	const float r = sqrt((index + 0.5f) / count);
	const float theta = index * GOLDEN_ANGLE + rotation;
	return r * float2(cos(theta), sin(theta));
}

float LinearizeDepth(float d, float near, float far)
{
	return (near * far) / (far - d * (far - near));
}

int SelectCascade(float viewDepth)
{
	for (uint c = 0; c < u_Pass.CascadeCount; ++c)
		if (viewDepth < u_Pass.CascadeSplits[c])
			return (int)c;

	return (int)u_Pass.CascadeCount - 1;
}

// PCSS for one cascade layer: blocker search -> penumbra estimate -> variable-kernel PCF
float SampleSunCascade(float3 worldPos, int cascade, float3 N, float3 L, float2 pixel)
{
	const float NdotL = clamp(dot(N, L), 0.0f, 1.0f);

	// Receiver reprojection along the normal. This, not the removed compare epsilon, is what keeps
	// sloped surfaces clean; the epsilon is now raster bias in the depth pass
	const float3 offsetPos = worldPos + N * (u_Pass.CascadeTexelWorld[cascade] * 2.0f * (1.0f - NdotL));

	const float4 lightSpace = mul(u_Pass.LightViewProj[cascade], float4(offsetPos, 1.0f));
	const float3 proj = lightSpace.xyz / lightSpace.w;

	const float2 uv = float2(proj.x, -proj.y) * 0.5f + 0.5f;
	const float receiver = proj.z;

	if (receiver > 1.0f)
		return 0.0f;

	const uint map = u_Pass.SunShadowIndex;
	const float texelUV = u_Pass.SunShadowTexel;
	const float texelWorld = u_Pass.CascadeTexelWorld[cascade];
	const float rotation = InterleavedGradientNoise(pixel) * 6.2831853f;
	const float searchRotation = InterleavedGradientNoiseAlt(pixel) * 6.2831853f; // its own field, or the two patterns reinforce

	float filterTexels;
	if (u_Pass.SoftShadows != 0)
	{
		// Only blockers inside the light's cone can shadow this point: at a gap of SEARCH_GAP_WORLD the
		// cone has radius LightSize * SEARCH_GAP_WORLD, and that one radius bounds both the search and
		// the filter. Searching wider only drags in occluders that cannot shadow this point (the
		// classic PCSS over-blur); filtering wider than we searched invents penumbra we never measured
		const float searchWorld = min(u_Pass.LightSize * SEARCH_GAP_WORLD, MAX_FILTER_WORLD);

		// The texel ceiling tracks sqrt(tap count) so a wider kernel always gets proportionally more
		// taps - otherwise the low sample counts turn the widest penumbrae into grain
		const float tapCeiling = MAX_FILTER_TEXELS * sqrt(float(u_Pass.PCFSamples) / PCF_REFERENCE_SAMPLES);
		const float maxFilterTexels = max(min(searchWorld / texelWorld, tapCeiling), MIN_FILTER_TEXELS);

		// 1. Blocker search reads raw depth through the point sampler: it needs the stored values, not
		// a bilinear blend that invents intermediate depths across a silhouette. The disk is rotated
		// per pixel: a fixed pattern keeps the penumbra size spatially coherent but also spatially
		// QUANTIZED, and those steps read as contour rings following the silhouette. Rotating trades
		// them for a little noise in the kernel width, which the PCF pass then filters over
		const float searchUV = maxFilterTexels * texelUV;
		float blockerSum = 0.0f;
		int blockerCount = 0;

		for (int i = 0; i < u_Pass.BlockerSamples; ++i)
		{
			const float2 off = VogelDisk(float(i), float(u_Pass.BlockerSamples), searchRotation) * searchUV;
			const float d = g_TextureArrays[NonUniformResourceIndex(map)]
								.SampleLevel(g_Samplers[SAMPLER_NEAREST_CLAMP], float3(uv + off, float(cascade)), 0.0f)
								.r;

			if (d < receiver)
			{
				blockerSum += d;
				blockerCount++;
			}
		}

		if (blockerCount == 0)
			return 0.0f; // no occluder -> fully lit

		const float avgBlocker = blockerSum / float(blockerCount);

		// 2. Penumbra (directional): depth gap -> world, scale by light size, cap in world, then to
		// texels. Capping in world and not in texels is what stops Resolution from acting as a
		// softness knob and stops the width from stepping at every cascade boundary
		const float worldGap = (receiver - avgBlocker) * u_Pass.CascadeDepthRange[cascade];
		const float penumbraWorld = min(worldGap * u_Pass.LightSize, searchWorld);
		filterTexels = clamp(penumbraWorld / texelWorld, MIN_FILTER_TEXELS, maxFilterTexels);
	}
	else
	{
		filterTexels = FIXED_PCF_TEXELS;
	}

	// 3. Variable-kernel PCF over the rotated Vogel disk. Each tap is a hardware compare with free 2x2
	// filtering, and the kernel is small enough that the taps overlap, so the sum is already continuous
	// - jittering the ring radii on top of that only injects noise larger than the steps it removes
	const float filterUV = filterTexels * texelUV;
	float sum = 0.0f;
	for (int i = 0; i < u_Pass.PCFSamples; ++i)
	{
		const float2 off = VogelDisk(float(i), float(u_Pass.PCFSamples), rotation) * filterUV;
		sum += 1.0f
			- g_TextureArrays[NonUniformResourceIndex(map)]
				  .SampleCmpLevelZero(g_ComparisonSamplers[SAMPLER_SHADOW_CMP], float3(uv + off, float(cascade)), receiver);
	}

	return sum / float(u_Pass.PCFSamples);
}

// Sun shadow with a blend across the cascade boundary to hide the resolution seam
float SunShadow(float3 worldPos, float viewDepth, float3 N, float3 L, float2 pixel)
{
	if (u_Pass.CascadeCount == 0)
		return 0.0f;

	const int cascade = SelectCascade(viewDepth);
	float shadow = SampleSunCascade(worldPos, cascade, N, L, pixel);

	if (cascade + 1 < (int)u_Pass.CascadeCount)
	{
		const float splitFar = u_Pass.CascadeSplits[cascade];
		const float prevSplit = cascade == 0 ? 0.0f : u_Pass.CascadeSplits[cascade - 1];
		const float band = (splitFar - prevSplit) * u_Pass.CascadeBlend;
		const float t = (viewDepth - (splitFar - band)) / max(band, 1e-4f);

		if (t > 0.0f)
			shadow = lerp(shadow, SampleSunCascade(worldPos, cascade + 1, N, L, pixel), clamp(t, 0.0f, 1.0f));
	}

	return shadow;
}

// PCSS for one spot: Fernando's blocker ratio in LINEARISED depth, since a perspective frustum's
// stored depth is not linear in world distance the way the sun's ortho slab is
float SpotShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	const int layer = (int)u_Pass.SpotShadowParams[index].w;
	const float lightSize = u_Pass.SpotShadowParams[index].y;

	const float3 lightPos = u_Pass.SpotLights[index].Position.xyz;
	const float3 spotDir = normalize(u_Pass.SpotLights[index].Direction.xyz);
	const float far = max(u_Pass.SpotLights[index].Params.x, SPOT_SHADOW_MIN_FAR);
	const float outerCos = clamp(u_Pass.SpotLights[index].Params.z, 1e-3f, 1.0f);
	const float tanHalf = sqrt(max(1.0f - outerCos * outerCos, 0.0f)) / outerCos; // half-FOV is the outer cutoff

	// Distance along the spot axis IS the receiver's view-space depth in the light's frustum, so it
	// gives both the linear receiver and the texel world size without projecting first
	const float zView = max(dot(worldPos - lightPos, spotDir), SPOT_SHADOW_NEAR);
	const float texelUV = u_Pass.SpotShadowTexel;
	const float texelWorld = max(2.0f * zView * tanHalf * texelUV, 1e-6f);

	const float NdotL = clamp(dot(N, L), 0.0f, 1.0f);
	const float3 offsetPos = worldPos + N * (texelWorld * 2.0f * (1.0f - NdotL));

	const float4 lightSpace = mul(u_Pass.SpotLightViewProj[layer], float4(offsetPos, 1.0f));
	if (lightSpace.w <= 0.0f)
		return 0.0f; // behind the light

	const float3 proj = lightSpace.xyz / lightSpace.w;

	const float2 uv = float2(proj.x, -proj.y) * 0.5f + 0.5f; // Y-flipped viewport; Z is already 0..1
	const float receiver = proj.z;

	if (receiver > 1.0f)
		return 0.0f;

	const uint map = u_Pass.SpotShadowIndex;
	const float rotation = InterleavedGradientNoise(pixel) * 6.2831853f;
	const float searchRotation = InterleavedGradientNoiseAlt(pixel) * 6.2831853f;

	float filterTexels;
	if (u_Pass.SoftShadows != 0)
	{
		// Same bound as the sun: only blockers inside the light's cone can shadow this point, and the
		// one radius bounds both the search and the filter
		const float searchWorld = min(lightSize * SEARCH_GAP_WORLD, MAX_FILTER_WORLD);
		const float tapCeiling = MAX_FILTER_TEXELS * sqrt(float(u_Pass.PCFSamples) / PCF_REFERENCE_SAMPLES);
		const float maxFilterTexels = max(min(searchWorld / texelWorld, tapCeiling), MIN_FILTER_TEXELS);

		const float searchUV = maxFilterTexels * texelUV;
		float blockerSum = 0.0f;
		int blockerCount = 0;

		for (int i = 0; i < u_Pass.BlockerSamples; ++i)
		{
			const float2 off = VogelDisk(float(i), float(u_Pass.BlockerSamples), searchRotation) * searchUV;
			const float d = g_TextureArrays[NonUniformResourceIndex(map)]
								.SampleLevel(g_Samplers[SAMPLER_NEAREST_CLAMP], float3(uv + off, float(layer)), 0.0f)
								.r;

			if (d < receiver)
			{
				blockerSum += d;
				blockerCount++;
			}
		}

		if (blockerCount == 0)
			return 0.0f; // no occluder -> fully lit

		const float linBlocker = max(LinearizeDepth(blockerSum / float(blockerCount), SPOT_SHADOW_NEAR, far), 1e-4f);
		const float penumbraWorld = min((zView - linBlocker) / linBlocker * lightSize, searchWorld);
		filterTexels = clamp(penumbraWorld / texelWorld, MIN_FILTER_TEXELS, maxFilterTexels);
	}
	else
	{
		filterTexels = FIXED_PCF_TEXELS;
	}

	const float filterUV = filterTexels * texelUV;
	float sum = 0.0f;
	for (int i = 0; i < u_Pass.PCFSamples; ++i)
	{
		const float2 off = VogelDisk(float(i), float(u_Pass.PCFSamples), rotation) * filterUV;
		sum += 1.0f
			- g_TextureArrays[NonUniformResourceIndex(map)]
				  .SampleCmpLevelZero(g_ComparisonSamplers[SAMPLER_SHADOW_CMP], float3(uv + off, float(layer)), receiver);
	}

	return sum / float(u_Pass.PCFSamples);
}

float PointShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}
