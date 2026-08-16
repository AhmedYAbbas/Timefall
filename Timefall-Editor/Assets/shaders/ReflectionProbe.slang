//--------------------------
// - Timefall -
// Binding-model canary. Compiled and reflected, never drawn. Mirrors the design's
// set 0 per-frame / set 2 bindless tiers.
// --------------------------

struct CameraData
{
	float4x4 ViewProjection;
	float3 Position;
	float Padding;
}

struct Push
{
	uint TransformMatrix;
	uint MaterialIndex;
	int EntityID;
}

[[vk::binding(0, 0)]] ConstantBuffer<CameraData> u_Camera;
[[vk::binding(0, 2)]] Texture2D g_Textures[];
[[vk::binding(1, 2)]] SamplerState g_Sampler;

[[vk::push_constant]]
ConstantBuffer<Push> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
}

[shader("vertex")]
VSOutput vsMain(float3 position, float2 uv)
{
	VSOutput output;
	output.position = mul(u_Camera.ViewProjection, float4(position, 1.0));
	output.uv = uv;
	return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
	return g_Textures[NonUniformResourceIndex(u_Push.MaterialIndex)].Sample(g_Sampler, input.uv);
}
