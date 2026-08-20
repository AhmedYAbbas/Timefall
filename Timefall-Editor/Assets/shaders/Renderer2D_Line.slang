//--------------------------
// - Timefall -
// Renderer2D line list. Width and smoothing are rasterizer state, not geometry.
// --------------------------

import Common.Bindings;

struct Push
{
	float4x4 ViewProjection;
}

[[vk::push_constant]]
ConstantBuffer<Push> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float4 color : COLOR0;
	nointerpolation int entityID : TEXCOORD0;
}

struct FSOutput
{
	float4 color : SV_Target;
	int entityID : SV_Target1;
}

[shader("vertex")]
VSOutput vsMain(float3 position, float4 color, int entityID)
{
	VSOutput output;
	output.position = mul(u_Push.ViewProjection, float4(position, 1.0));
	output.color = color;
	output.entityID = entityID;
	return output;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	FSOutput output;
	output.color = input.color;
	output.entityID = input.entityID;
	return output;
}
