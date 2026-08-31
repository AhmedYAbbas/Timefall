//--------------------------
// - Timefall -
// Forward PBR
// --------------------------

import Common.Bindings;
import Common.IBL;
import Common.Shadows;
import Common.BRDF;
import Common.Iridescence;

#define SHOWCASE_IRIDESCENCE 1

// Reached by device address, not by descriptor. Slang lays these out in its *natural* layout
// through a pointer, so no float3x3 or bare float3 members: they would pack to 12-byte strides
// and break the array's 16-byte alignment. Measured ArrayStride: 112 and 80.
struct GpuTransform
{
	float4x4 Model;
	float4 Normal0; // transpose(inverse(mat3(model))), CPU-computed
	float4 Normal1;
	float4 Normal2;
}

struct GpuMaterial
{
	float4 BaseColorMetallic; // rgb = linear base color, a = metallic
	float4 EmissiveRoughness; // rgb = linear emissive, a = roughness
	float4 Params; // NormalStrength, EmissiveIntensity, Opacity, AlphaCutoff
	uint4 Maps0; // baseColor, normal, metallic, roughness
	uint4 Maps1; // ao, emissive, alphaMode, _pad
}

struct DrawPush
{
	ConstBufferPointer<GpuTransform> Transforms;
	ConstBufferPointer<GpuMaterial> Materials;
	uint TransformIndex;
	uint MaterialIndex;
	int EntityID;
	uint _Pad; 
}

[[vk::push_constant]]
ConstantBuffer<DrawPush> u_Draw;

struct VSOutput
{
	float4 position : SV_Position;
	float3 worldPos : TEXCOORD0;
	float2 uv : TEXCOORD1;
	float3x3 tbn : TEXCOORD2; // locations 2, 3, 4
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

[shader("vertex")]
VSOutput vsMain(float3 position, float3 normal, float2 texCoord, float3 tangent, float3 bitangent)
{
	const GpuTransform t = u_Draw.Transforms[u_Draw.TransformIndex];

	const float3x3 normalMatrix = transpose(float3x3(t.Normal0.xyz, t.Normal1.xyz, t.Normal2.xyz));

	const float4 world = mul(t.Model, float4(position, 1.0f));

	const float3 N = normalize(mul(normalMatrix, normal));
	float3 T = normalize(mul(normalMatrix, tangent));
	T = normalize(T - N * dot(N, T)); // Gram-Schmidt re-orthogonalize
	const float3 Bw = mul(normalMatrix, bitangent);
	const float3 B = cross(N, T) * sign(dot(cross(N, T), Bw)); // preserve UV handedness

	VSOutput o;
	o.position = mul(u_Pass.ViewProjection, world);
	o.worldPos = world.xyz;
	o.uv = texCoord;
	o.tbn = transpose(float3x3(T, B, N));
	return o;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	const GpuMaterial m = u_Draw.Materials[u_Draw.MaterialIndex];

	float3 mapN = SampleBindless(m.Maps0.y, SAMPLER_LINEAR_REPEAT, input.uv).rgb * 2.0 - 1.0;
	mapN.xy *= m.Params.x; // NormalStrength

	SurfaceParams s;
	s.N = normalize(mul(input.tbn, normalize(mapN)));
	s.V = normalize(u_Pass.CameraPosition.xyz - input.worldPos);

	const float4 baseSample = SampleBindless(m.Maps0.x, SAMPLER_LINEAR_REPEAT, input.uv);
	const float alpha = m.Params.z * baseSample.a; // texture alpha is linear even on sRGB maps
	if (m.Maps1.z == 1 && alpha < m.Params.w) // Mask: cut the fragment out entirely
		discard;

	s.Albedo = m.BaseColorMetallic.rgb * baseSample.rgb;
	// glTF packs metallic (B) + roughness (G) into one texture; a separately-authored grayscale
	// map has R=G=B, so these channels are correct for both cases
	s.Metallic = m.BaseColorMetallic.a * SampleBindless(m.Maps0.z, SAMPLER_LINEAR_REPEAT, input.uv).b;
	s.Roughness = clamp(m.EmissiveRoughness.a * SampleBindless(m.Maps0.w, SAMPLER_LINEAR_REPEAT, input.uv).g, 0.045, 1.0);
	s.Roughness = clamp(FilterRoughness(s.N, s.Roughness), 0.045, 1.0);
	s.AO = SampleBindless(m.Maps1.x, SAMPLER_LINEAR_REPEAT, input.uv).r;
	s.F0 = lerp(float3(0.04), s.Albedo, s.Metallic);

#if SHOWCASE_IRIDESCENCE

	// Soap bubble
	if (m.Maps0.x == BINDLESS_WHITE)
	{
		IridescenceParams ip;
		ip.ThicknessMin = 180.0;
		ip.ThicknessMax = 520.0;
		ip.FilmIOR = 1.33;
		ip.SwirlScale = 1.2;
		ip.FlowSpeed = 0.25;
		ip.Blend = 0.85;
		ip.RoughnessMin = 0.03;
		ip.RoughnessMax = 0.12;
		ip.Octaves = 4;

		ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	}

	// Oil slick
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 300.0;
	// 	ip.ThicknessMax = 1400.0;
	// 	ip.FilmIOR = 1.45;
	// 	ip.SwirlScale = 3.5;
	// 	ip.FlowSpeed = 0.5;
	// 	ip.Blend = 0.9;
	// 	ip.RoughnessMin = 0.05;
	// 	ip.RoughnessMax = 0.25;
	// 	ip.Octaves = 5;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }

	// Anodised titanium
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 389.0;
	// 	ip.ThicknessMax = 680.0;
	// 	ip.FilmIOR = 2.4;
	// 	ip.SwirlScale = 0.6;
	// 	ip.FlowSpeed = 0.05;
	// 	ip.Blend = 1.0;
	// 	ip.RoughnessMin = 0.12;
	// 	ip.RoughnessMax = 0.3;
	// 	ip.Octaves = 2;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }

	// Scarab carapace
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 420.0;
	// 	ip.ThicknessMax = 560.0;
	// 	ip.FilmIOR = 1.9;
	// 	ip.SwirlScale = 8.0;
	// 	ip.FlowSpeed = 0.15;
	// 	ip.Blend = 0.95;
	// 	ip.RoughnessMin = 0.04;
	// 	ip.RoughnessMax = 0.10;
	// 	ip.Octaves = 6;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }

	// CD / grating
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 900.0;
	// 	ip.ThicknessMax = 4200.0;
	// 	ip.FilmIOR = 1.5;
	// 	ip.SwirlScale = 0.4;
	// 	ip.FlowSpeed = 0.1;
	// 	ip.Blend = 1.0;
	// 	ip.RoughnessMin = 0.02;
	// 	ip.RoughnessMax = 0.06;
	// 	ip.Octaves = 1;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }

	// Ghost sheen
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 250.0;
	// 	ip.ThicknessMax = 700.0;
	// 	ip.FilmIOR = 1.35;
	// 	ip.SwirlScale = 2.0;
	// 	ip.FlowSpeed = 0.3;
	// 	ip.Blend = 0.35;
	// 	ip.RoughnessMin = 0.05;
	// 	ip.RoughnessMax = 0.40;
	// 	ip.Octaves = 4;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }

	// Molten
	// if (m.Maps0.x == BINDLESS_WHITE)
	// {
	// 	IridescenceParams ip;
	// 	ip.ThicknessMin = 200.0;
	// 	ip.ThicknessMax = 2500.0;
	// 	ip.FilmIOR = 1.4;
	// 	ip.SwirlScale = 0.8;
	// 	ip.FlowSpeed = 2.5;
	// 	ip.Blend = 1.0;
	// 	ip.RoughnessMin = 0.10;
	// 	ip.RoughnessMax = 0.50;
	// 	ip.Octaves = 3;

	// 	ApplyIridescence(s, input.worldPos, u_Frame.Time, ip);
	// }
	
#endif

	const float viewDepth = -mul(u_Pass.View, float4(input.worldPos, 1.0)).z; // view looks down -z

	float3 Lo = EvaluateAmbient(s);

	for (uint i = 0; i < u_Pass.DirCount; i++)
	{
		const float3 L = normalize(-u_Pass.DirLights[i].Direction.xyz);
		const float3 radiance = u_Pass.DirLights[i].Color.rgb * u_Pass.DirLights[i].Color.a;

		const float shadow = i == 0 ? SunShadow(input.worldPos, viewDepth, s.N, L) : 0.0;
		Lo += (1.0 - shadow) * CookTorrance(s, L, radiance);
	}

	for (uint i = 0; i < u_Pass.PointCount; i++)
	{
		const float3 toLight = u_Pass.PointLights[i].Position.xyz - input.worldPos;
		const float dist = length(toLight);
		const float3 L = toLight / max(dist, 0.0001);
		const float att = Attenuate(dist, u_Pass.PointLights[i].Position.w);

		float3 radiance = u_Pass.PointLights[i].Color.rgb * u_Pass.PointLights[i].Color.a * att;
		if (u_Pass.PointShadowParams[i].x > 0.5)
			radiance *= 1.0 - PointShadow(input.worldPos, (int)i, s.N, L);

		Lo += CookTorrance(s, L, radiance);
	}

	for (uint i = 0; i < u_Pass.SpotCount; i++)
	{
		const float range = u_Pass.SpotLights[i].Params.x;
		const float innerCos = u_Pass.SpotLights[i].Params.y;
		const float outerCos = u_Pass.SpotLights[i].Params.z;
		const float intensity = u_Pass.SpotLights[i].Params.w;

		const float3 toLight = u_Pass.SpotLights[i].Position.xyz - input.worldPos;
		const float dist = length(toLight);
		const float3 L = toLight / max(dist, 0.0001);
		const float att = Attenuate(dist, range);

		const float3 spotDir = normalize(u_Pass.SpotLights[i].Direction.xyz);
		const float theta = dot(-L, spotDir); // 1 when the fragment is on the cone axis
		const float cone = clamp((theta - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);

		float3 radiance = u_Pass.SpotLights[i].Color.rgb * intensity * att * cone;
		if (u_Pass.SpotShadowParams[i].x > 0.5)
			radiance *= 1.0 - SpotShadow(input.worldPos, (int)i, s.N, L);

		Lo += CookTorrance(s, L, radiance);
	}

	Lo += m.EmissiveRoughness.rgb * m.Params.y * SampleBindless(m.Maps1.y, SAMPLER_LINEAR_REPEAT, input.uv).rgb;

	FSOutput o;
	o.color = float4(Lo, m.Maps1.z == 2 ? alpha : 1.0); // only Blend writes a real alpha
	o.entityID = u_Draw.EntityID;
	return o;
}
