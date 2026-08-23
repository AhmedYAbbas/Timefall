//--------------------------
// - Timefall -
// Equirectangular HDR -> one cube face. The unit cube is drawn once per face with that face's
// view-projection in the push block
// --------------------------


import Common.Bindings;

struct EquirectPush
{
	float4x4 ViewProjection;
	uint EquirectIndex;
	uint _Pad0;
	uint _Pad1;
	uint _Pad2;
}

[[vk::push_constant]]
ConstantBuffer<EquirectPush> u_Push;

struct VSOutput
{
	float4 position : SV_Position;
	float3 localPos : TEXCOORD0;
}

// All five MeshVertex attributes are declared because the pipeline validates the declared count
// against its vertex layout; only position is used
[shader("vertex")]
VSOutput vsMain(float3 position, float3 normal, float2 texCoord, float3 tangent, float3 bitanget)
{
	VSOutput o;
	o.localPos = position;
	o.position = mul(u_Push.ViewProjection, float4(position, 1.0f));
	return o;
}

static const float2 c_InvAtan = float2(0.1591, 0.3183); // 1/(2pi), 1/pi
static const float c_MaxRadiance = 1.0e6; // safety ceiling against pathological source texels

float2 SampleSphericalMap(float3 dir)
{
	float2 uv = float2(atan2(dir.z, dir.x), asin(dir.y));
	uv *= c_InvAtan;
	uv += 0.5f;
	return uv;
}

[shader("fragment")]
float4 fsMain(VSOutput i) : SV_Target
{
	const float3 dir = normalize(i.localPos);
	const float3 color = SampleBindless(u_Push.EquirectIndex, SAMPLER_LINEAR_REPEAT, SampleSphericalMap(dir)).rgb;
	return float4(clamp(color, float3(0.0f), float3(c_MaxRadiance)), 1.0f);
}
