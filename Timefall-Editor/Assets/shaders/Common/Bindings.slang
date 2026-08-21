//--------------------------
// - Timefall -
// The engine's canonical descriptor layout. Every declaration here has a twin in VulkanBindings.cpp
// and the two must agree - nothing checks it but the pixels
// --------------------------

// set 0 - per frame. One 256-byte slice per frame in flight, selected by a dynamic offset. Fields are
// appended, never reordered: the C++ side writes a matching struct and only offsets are contractual
struct FrameUniforms
{
	float Time;
}

[[vk::binding(0, 0)]]
ConstantBuffer<FrameUniforms> u_Frame;

// set 1 - per pass. 4496 bytes in a 4608-byte slice, selected by a dynamic offset CommandList records.
// Twin of PassUniforms.h - fields are appended, never reordered
struct DirLight { float4 Direction; float4 Color; }
struct PointLight { float4 Position; float4 Color; }
struct SpotLight { float4 Position; float4 Direction; float4 Color; float4 Params; }

struct PassUniforms
{
	float4x4 ViewProjection;
	float4x4 View;
	float4 CameraPosition;

	DirLight DirLights[4];
	PointLight PointLights[32];
	SpotLight SpotLights[16];
	uint DirCount;
	uint PointCount;
	uint SpotCount;
	uint _LightPad;

	float EnvIntensity;
	float EnvRotation;
	float MaxReflectionLod;
	int HasEnvironment;
	uint IrradianceIndex;
	uint PrefilterIndex;
	uint SkyboxIndex;
	uint _EnvPad;

	float4x4 LightViewProj[4];
	float4 CascadeSplits;
	float4 CascadeTexelWorld;
	float4 CascadeDepthRange;
	uint CascadeCount;
	uint VisualizeCascades;
	float LightSize;
	float DepthBias;
	float CascadeBlend;
	int BlockerSamples;
	int PCFSamples;
	int SoftShadows;

	float4x4 SpotLightViewProj[16];
	float4 SpotShadowParams[16];

	float4 PointShadowParams[32];
}

[[vk::binding(0, 1)]]
ConstantBuffer<PassUniforms> u_Pass;

// set 2 - bindless. g_Textures sits at binding 15 because a variable-count binding must be the
// highest in its set, leaving 1-14 for the material and transform buffers
static const uint SAMPLER_LINEAR_REPEAT = 0;
static const uint SAMPLER_LINEAR_CLAMP = 1;
static const uint SAMPLER_NEAREST_CLAMP = 2;

static const uint BINDLESS_WHITE = 0;

[[vk::binding(0, 2)]]
SamplerState g_Samplers[3];

[[vk::binding(15, 2)]]
Texture2D g_Textures[];


float4 SampleBindless(uint index, uint samplerSlot, float2 uv)
{
	return g_Textures[NonUniformResourceIndex(index)].Sample(g_Samplers[samplerSlot], uv);
}
