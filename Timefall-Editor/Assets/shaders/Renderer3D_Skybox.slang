//--------------------------
// - Timefall -
// The environment cube drawn at depth 1.0, filling only where nothing else was. The camera sits
// inside the unit cube, so the pipeline culls front faces
// --------------------------

import Common.Bindings;

struct SkyboxPush
{
	float4x4 SkyViewProjection;
	uint SkyboxIndex;
	float EnvRotation;
	uint _Pad0;
	uint _Pad1;
}

[[vk::push_constant]]
ConstantBuffer<SkyboxPush> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float3 dir : TEXCOORD0;
}

float3 RotateY(float3 v, float angle)
{
	const float c = cos(angle);
	const float s = sin(angle);
	return float3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

// All five MeshVertex attributes are declared because the pipeline validates the declared count
// against its vertex layout; only position is used
[shader("vertex")]
VSOutput vsMain(float3 position, float3 normal, float2 texCoord, float3 tangent, float3 bitangent)
{
	VSOutput o;
	o.dir = RotateY(position, u_Push.EnvRotation);

	const float4 clip = mul(u_Push.SkyViewProjection, float4(position, 1.0f));
	o.position = clip.xyww;

	return o;
}

struct FSOutput
{
	float4 color : SV_Target0;
	int entityID : SV_Target1;
}

[shader("fragment")]
FSOutput fsMain(VSOutput input)
{
	FSOutput o;
	o.color = float4(SampleBindlessCube(u_Push.SkyboxIndex, SAMPLER_LINEAR_CLAMP, normalize(input.dir)).rgb, 1.0f);
	o.entityID = -1;

	return o;
}
