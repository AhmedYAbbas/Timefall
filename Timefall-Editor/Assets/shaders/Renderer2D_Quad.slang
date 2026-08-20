//--------------------------
// - Timefall -
// Renderer2D quad batch. The bindless slot rides in the vertex, so one batch draws any number of
// distinct textures and breaks only when it runs out of vertices.
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
	nointerpolation uint texIndex : TEXCOORD1;
	float tiling : TEXCOORD2;
	nointerpolation int entityID : TEXCOORD3;
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

[shader("vertex")]
VSOutput vsMain(float3 position, float4 color, float2 texCoord, uint texIndex, float tiling, int entityID)
{
	VSOutput output;
	output.position = mul(u_Push.ViewProjection, float4(position, 1.0));
	output.color = color;
	output.uv = texCoord;
	output.texIndex = texIndex;
	output.tiling = tiling;
	output.entityID = entityID;
	return output;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	const float4 textureColor = SampleBindless(input.texIndex, SAMPLER_LINEAR_REPEAT, input.uv * input.tiling);
	if (textureColor.a == 0.0)
		discard;

	FSOutput output;
	output.color = textureColor * input.color;
	output.entityID = input.entityID;
	return output;
}
