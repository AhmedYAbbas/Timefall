//--------------------------
// - Timefall -
// Forward PBR
// --------------------------

import Common.Bindings;
import Common.IBL;
import Common.Shadows;
import Common.BRDF;

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

	float3 Lo = EvaluateAmbient(s);

	FSOutput o;
	o.color = float4(Lo, m.Maps1.z == 2 ? alpha : 1.0); // only Blend writes a real alpha
	o.entityID = u_Draw.EntityID;
	return o;
}
