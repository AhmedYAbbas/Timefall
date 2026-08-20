//--------------------------
// - Timefall -
// Renderer2D MSDF text. The atlas index is per-vertex, so two fonts can share one batch
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
	float2 uv : TEXCOORD0;
	nointerpolation uint atlasIndex : TEXCOORD1;
	nointerpolation int entityID : TEXCOORD2;
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

float Median(float r, float g, float b)
{
	return max(min(r, g), min(max(r, g), b));
}

float ScreenPxRange(uint atlasIndex, float2 uv)
{
	const float pxRange = 2.0; // must match the distance field the atlas was generated with

	uint width, height;
	g_Textures[NonUniformResourceIndex(atlasIndex)].GetDimensions(width, height);

	const float2 unitRange = float2(pxRange, pxRange) / float2(width, height);
	const float2 screenTexSize = float2(1.0, 1.0) / fwidth(uv);
	return max(0.5 * dot(unitRange, screenTexSize), 1.0);
}

[shader("vertex")]
VSOutput vsMain(float3 position, float4 color, float2 texCoord, uint atlasIndex, int entityID)
{
	VSOutput output;
	output.position = mul(u_Push.ViewProjection, float4(position, 1.0));
	output.color = color;
	output.uv = texCoord;
	output.atlasIndex = atlasIndex;
	output.entityID = entityID;
	return output;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	const float3 msd = SampleBindless(input.atlasIndex, SAMPLER_LINEAR_CLAMP, input.uv).rgb;
	const float sd = Median(msd.r, msd.g, msd.b);
	const float screenPxDistance = ScreenPxRange(input.atlasIndex, input.uv) * (sd - 0.5);
	const float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);

	if (opacity == 0.0)
		discard;

	FSOutput output;
	output.color = float4(input.color.rgb, input.color.a * opacity);
	output.entityID = input.entityID;
	return output;
}
