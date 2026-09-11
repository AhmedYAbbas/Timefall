//--------------------------
// - Timefall -
// Depth-only shadow pass for sun, spot and point. One multiview draw fills every view; SV_ViewID picks the matrix or cube face
// --------------------------

import Common.Bindings;
import Common.Shadows;


// Twin of GpuTransform in Renderer3D_Lit.slang - the same array, reached by the same device address
struct GpuTransform
{
	float4x4 Model;
	float4 Normal0;
	float4 Normal1;
	float4 Normal2;
}

static const uint SHADOW_MODE_SUN = 0;
static const uint SHADOW_MODE_SPOT = 1;
static const uint SHADOW_MODE_POINT = 2;

// Rows map (worldPos - lightPos) to (x = sc, y = -tc, depth = |ma|) under the Y-flipped viewport: GL's lookAt faces with the up row negated
static const float3x3 CUBE_FACE_BASIS[6] = {
	float3x3( 0, 0, -1, 0, 1,  0,  1,  0,  0),	// +X
	float3x3( 0, 0,  1, 0, 1,  0, -1,  0,  0),	// -X
	float3x3( 1, 0,  0, 0, 0, -1,  0,  1,  0),	// +Y
	float3x3( 1, 0,  0, 0, 0,  1,  0, -1,  0),	// -Y
	float3x3( 1, 0,  0, 0, 1,  0,  0,  0,  1),	// +Z
	float3x3(-1, 0,  0, 0, 1,  0,  0,  0, -1),	// -Z
};

// Shared by all three shadow types. LightPosFar is dead for sun and spot
struct ShadowPush
{
	ConstBufferPointer<GpuTransform> Transforms;
	uint TransformIndex;
	uint ViewBase;
	float4 LightPosFar; // point only: xyz = light position, w = raw light range (PointShadowFar clamps it)
	uint Mode;
	uint _Pad0;
	uint _Pad1;
	uint _Pad2;
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

	if (u_Shadow.Mode == SHADOW_MODE_POINT)
	{
		// 90-degree faces need no x/y scale; z stays affine in depth so the divide yields A + B / depth with no NaN at the light
		const float3 v = mul(CUBE_FACE_BASIS[viewID], world.xyz - u_Shadow.LightPosFar.xyz);
		const float2 depth = PerspectiveDepthCoeffs(POINT_SHADOW_NEAR, PointShadowFar(u_Shadow.LightPosFar.w));
		return float4(v.x, v.y, depth.x * v.z + depth.y, v.z);
	}

	const float4x4 lightViewProj = u_Shadow.Mode == SHADOW_MODE_SPOT ? u_Pass.SpotLightViewProj[u_Shadow.ViewBase + viewID] : u_Pass.LightViewProj[u_Shadow.ViewBase + viewID];
	return mul(lightViewProj, world);
}

// Empty, but required: the pipeline always builds two stages
[shader("fragment")]
void fsMain()
{
}
