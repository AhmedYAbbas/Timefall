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

// Cubes cannot share g_Textures: a set may have only one variable-count binding and it must be the
// highest, so cubes take a fixed-size array below it. Twin of VulkanBindings::kTextureCubeCapacity
[[vk::binding(1, 2)]]
TextureCube g_TextureCubes[64];

static const uint BINDLESS_WHITE_CUBE = 0;

// Array textures take their own fixed-size bindings for the same reason cubes do. Twins of
// VulkanBindings::kTextureArrayCapacity and kTextureCubeArrayCapacity
[[vk::binding(2, 2)]]
Texture2DArray g_TextureArrays[16];

static const uint BINDLESS_WHITE_ARRAY = 0;

[[vk::binding(3, 2)]]
TextureCubeArray g_TextureCubeArrays[8];

static const uint BINDLESS_WHITE_CUBE_ARRAY = 0;

// Comparison sampler for shadow taps: SampleCmp does the depth compare in the sampler and returns
// hardware-filtered occlusion. The PCSS blocker search still reads raw depth through g_Samplers
[[vk::binding(4, 2)]]
SamplerComparisonState g_ComparisonSamplers[1];

static const uint SAMPLER_SHADOW_CMP = 0;

[[vk::binding(15, 2)]]
Texture2D g_Textures[];


float4 SampleBindless(uint index, uint samplerSlot, float2 uv)
{
	return g_Textures[NonUniformResourceIndex(index)].Sample(g_Samplers[samplerSlot], uv);
}

float4 SampleBindlessCube(uint index, uint samplerSlot, float3 dir)
{
	return g_TextureCubes[NonUniformResourceIndex(index)].Sample(g_Samplers[samplerSlot], dir);
}

float4 SampleBindlessCubeLod(uint index, uint samplerSlot, float3 dir, float lod)
{
	return g_TextureCubes[NonUniformResourceIndex(index)].SampleLevel(g_Samplers[samplerSlot], dir, lod);
}

float4 SampleBindlessArray(uint index, uint samplerSlot, float2 uv, uint layer)
{
	return g_TextureArrays[NonUniformResourceIndex(index)].Sample(g_Samplers[samplerSlot], float3(uv, layer));
}

float4 SampleBindlessCubeArray(uint index, uint samplerSlot, float3 dir, float cube)
{
	return g_TextureCubeArrays[NonUniformResourceIndex(index)].Sample(g_Samplers[samplerSlot], float4(dir, cube));
}
