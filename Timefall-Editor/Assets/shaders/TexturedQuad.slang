//--------------------------
// - Timefall -
// Bring-up: a bindless texture index and a sampler slot in push constants, the view-projection from
// set 0. Three quads share one pipeline and one bound descriptor set; only the push block changes.
// --------------------------

import Common.Bindings;

// 20 bytes, and the C++ QuadPush struct in EditorLayer.cpp must match byte for byte: float2 aligns to
// 8 at offset 0, so Center 0..7, Scale 8..11, TextureIndex 12..15, SamplerSlot 16..19.
struct Push
{
	float2 Center;
	float Scale;
	uint TextureIndex;
	uint SamplerSlot;
}

[[vk::push_constant]]
ConstantBuffer<Push> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

[shader("vertex")]
VSOutput vsMain(float2 position, float2 texCoord)
{
	const float3 world = float3(position * u_Push.Scale + u_Push.Center, 0.0);

	VSOutput output;
	output.position = mul(u_Frame.ViewProjection, float4(world, 1.0));
	output.uv = texCoord;
	return output;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	FSOutput output;
	output.color = SampleBindless(u_Push.TextureIndex, u_Push.SamplerSlot, input.uv);
	output.entityID = -1;
	return output;
}
