//--------------------------
// - Timefall -
// Renderer2D analytic circle. The quad's local-space position doubles as the distance field, so
// thickness and fade stay exact at any zoom.
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
	float2 localPosition : TEXCOORD0;
	float4 color : COLOR0;
	float thickness : TEXCOORD1;
	float fade : TEXCOORD2;
	nointerpolation int entityID : TEXCOORD3;
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

[shader("vertex")]
VSOutput vsMain(float3 worldPosition, float2 localPosition, float4 color, float thickness, float fade, int entityID)
{
	VSOutput output;
	output.position = mul(u_Push.ViewProjection, float4(worldPosition, 1.0));
	output.localPosition = localPosition;
	output.color = color;
	output.thickness = thickness;
	output.fade = fade;
	output.entityID = entityID;
	return output;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	const float distance = 1.0 - length(input.localPosition);

	float circleAlpha = smoothstep(0.0, input.fade, distance);
	circleAlpha *= smoothstep(input.thickness + input.fade, input.thickness, distance);

	if (circleAlpha == 0.0)
		discard;

	FSOutput output;
	output.color = float4(input.color.rgb, input.color.a * circleAlpha);
	output.entityID = input.entityID;
	return output;
}
