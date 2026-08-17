//--------------------------
// - Timefall -
// Bring-up geometry. Vertex buffer, no descriptors - push constants only.
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

[shader("vertex")]
VSOutput vsMain(float3 position, float3 color)
{
	VSOutput output;
	output.position = float4(position, 1.0);
	output.color = color;
	return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
	return float4(input.color * u_Push.Tint, 1.0);
}
