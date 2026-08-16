//--------------------------
// - Timefall -
// Bring-up triangle. No vertex buffer, no descriptors - push constants only.
// --------------------------

struct Push
{
	float3 Tint;
	float Time;
}

[[vk::push_constant]]
ConstantBuffer<Push> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float3 color : COLOR0;
}

static const float2 k_Positions[3] = { float2(0.0, -0.6), float2(0.6, 0.6), float2(-0.6, 0.6) };
static const float3 k_Colors[3] = { float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0) };

[shader("vertex")]
VSOutput vsMain(uint vertexID : SV_VertexID)
{
	VSOutput output;
	output.position = float4(k_Positions[vertexID], 0.0, 1.0);
	output.color = k_Colors[vertexID];
	return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
	return float4(input.color * u_Push.Tint, 1.0);
}
