//--------------------------
// - Timefall -
// Depth-only shadow pass. One multiview draw fills every cascade layer; SV_ViewID picks the matrix
// --------------------------

import Common.Bindings;


// Twin of GpuTransform in Renderer3D_Lit.slang - the same array, reached by the same device address
struct GpuTransform
{
	float4x4 Model;
	float4 Normal0;
	float4 Normal1;
	float4 Normal2;
}

// Shared by all three shadow types. LightPosFar is dead for sun and spot
struct ShadowPush
{
	ConstBufferPointer<GpuTransform> Transforms;
	uint TransformIndex;
	uint ViewBase;
	float4 LightPosFar; // xyz = light position, w = far plane distance for point/spot
}

[[vk::push_constant]]
ConstantBuffer<ShadowPush> u_Shadow;

// All five MeshVertex attributes are declared because the pipeline validates the declared count
// against its vertex layout; only position is used
[shader("vertex")]
float4 vsMain(float3 position, float3 normal, float2 texCoord, float3 tangent, float3 bitangent, uint viewID : SV_ViewID) : SV_Position
{
	const GpuTransform t = u_Shadow.Transforms[u_Shadow.TransformIndex];
	const float4 world = mul(t.Model, float4(position, 1.0f));

	return mul(u_Pass.LightViewProj[u_Shadow.ViewBase + viewID], world);
}

// Empty, but required: the pipeline always builds two stages
[shader("fragment")]
void fsMain()
{
}
