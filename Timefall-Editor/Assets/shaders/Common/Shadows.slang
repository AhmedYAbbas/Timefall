import Common.Bindings;

static const float BLOCKER_SEARCH_TEXELS = 24.0f;
static const float MAX_FILTER_TEXELS = 24.0f;
static const float FIXED_PCF_TEXELS = 2.0f; // fixed-kernel radius when soft shadows are off
static const float GOLDEN_ANGLE = 2.39996323f;

// Interleaved gradient noise (Jimenez 2014): low-discrepancy per-pixel rotation seed
float InterleavedGradientNoise(float2 p)
{
	return frac(52.9829189f * frac(dot(p, float2(0.06711056f, 0.00583715f))));
}

// Evenly-distributed disk sample (Vogel/sunflower), rotated per pixel
float2 VogelDisk(int i, int count, float rotation)
{
	const float r = sqrt((float(i) + 0.5f) / float(count));
	const float theta = float(i) * GOLDEN_ANGLE + rotation;
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
	const float rotation = InterleavedGradientNoise(pixel) * 6.2831853f;

	float filterUV;
	if (u_Pass.SoftShadows != 0)
	{
		// 1. Blocker search reads raw depth through the plain sampler: it needs the stored values,
		// not an occlusion fraction. Fixed (un-rotated) pattern so the penumbra SIZE stays spatially
		// coherent; rotating here makes the kernel size flicker and no tap count fixes that
		const float searchUV = BLOCKER_SEARCH_TEXELS * texelUV;
		float blockerSum = 0.0f;
		int blockerCount = 0;

		for (int i = 0; i < u_Pass.BlockerSamples; ++i)
		{
			const float2 off = VogelDisk(i, u_Pass.BlockerSamples, 0.0f) * searchUV;
			const float d = g_TextureArrays[NonUniformResourceIndex(map)]
								.SampleLevel(g_Samplers[SAMPLER_LINEAR_CLAMP], float3(uv + off, float(cascade)), 0.0f)
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

		// 2. Penumbra (directional): depth gap -> world, scale by light size, back to texels
		const float worldGap = (receiver - avgBlocker) * u_Pass.CascadeDepthRange[cascade];
		const float penumbraTexels = (worldGap * u_Pass.LightSize) / u_Pass.CascadeTexelWorld[cascade];
		filterUV = clamp(penumbraTexels, 1.0f, MAX_FILTER_TEXELS) * texelUV;
	}
	else
	{
		filterUV = FIXED_PCF_TEXELS * texelUV;
	}

	// 3. Variable-kernel PCF over the rotated Vogel disk. Each tap is a hardware compare with free 2x2 filtering
	float sum = 0.0f;
	for (int i = 0; i < u_Pass.PCFSamples; ++i)
	{
		const float2 off = VogelDisk(i, u_Pass.PCFSamples, rotation) * filterUV;
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

float SpotShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}

float PointShadow(float3 worldPos, int index, float3 N, float3 L, float2 pixel)
{
	return 0.0f;
}
