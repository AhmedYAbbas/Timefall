#include "tfpch.h"
#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/ShaderLibrary.h"
#include "Timefall/Renderer/GPUProfiler.h"
#include "Timefall/Renderer/PassUniforms.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/Environment.h"

#include "Timefall/Debug/PerformanceStats.h"

#include "Timefall/Asset/AssetManager.h"
#include "Timefall/Asset/EditorAssetManager.h"

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/RHI/Texture.h"
#include "Timefall/RHI/FrameAllocator.h"
#include "Timefall/RHI/Bindings.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Timefall
{
	// Twin of GpuTransform in Renderer3D_Lit.slang. Measured ArrayStride 112, members at 0/64/80/96.
	// The normal basis is three vec4 columns, not a mat3: Slang packs a float3x3 scalar-tight through
	// a ConstBufferPointer, which would leave the array 100 B strided and unaligned.
	struct GpuTransform
	{
		glm::mat4 Model;
		glm::vec4 Normal0;
		glm::vec4 Normal1;
		glm::vec4 Normal2;
	};
	static_assert(sizeof(GpuTransform) == 112);

	// Twin of DrawPush. 32 B of the shared 128-byte range.
	struct DrawPush
	{
		uint64_t Transforms = 0;
		uint64_t Materials = 0;
		uint32_t TransformIndex = 0;
		uint32_t MaterialIndex = 0;
		int32_t EntityID = -1;
		uint32_t _Pad = 0;
	};
	static_assert(sizeof(DrawPush) == 32);

	struct ShadowPush
	{
		uint64_t Transforms = 0;
		uint32_t TransformIndex = 0;
		uint32_t ViewBase = 0;
		glm::vec4 LightPosFar{0.0f};
	};
	static_assert(sizeof(ShadowPush) == 32);

	struct SkyboxPush
	{
		glm::mat4 SkyViewProjection{1.0f};
		uint32_t SkyboxIndex = 0;
		float EnvRotation = 0.0f;
		uint32_t _Pad[2]{};
	};
	static_assert(sizeof(SkyboxPush) == 80);

	struct GpuMaterial
	{
		glm::vec4 BaseColorMetallic;
		glm::vec4 EmissiveRoughness;
		glm::vec4 Params;
		glm::uvec4 Maps0;
		glm::uvec4 Maps1;
	};
	static_assert(sizeof(GpuMaterial) == 80);

	struct MeshSubmission
	{
		glm::mat4 Transform;
		Ref<MeshSource> Mesh;
		uint32_t SubmeshIndex;
		Ref<Material> Material;
		int EntityID;
	};

	struct PointCaster
	{
		glm::vec3 Position;
		float Far;
		uint32_t Layer;
	};

	// Mirros ResolvePush in Renderer3D_HDRResolve.slang. 16 bytes
	struct ResolvePush
	{
		uint32_t HDRColorIndex = 0;
		float ExposureEV = 0.0f;
		int32_t Operator = 0;
		float WhitePoint = 4.0f;
	};

	struct Renderer3DData
	{
		Ref<RHI::RenderTarget> LDRTarget; // The LDR target the layer name; not owned
		Ref<RHI::RenderTarget> HDRTarget;

		Ref<Shader> ResolveShader;
		Ref<RHI::GraphicsPipeline> ResolvePipeline;

		uint32_t HDRColorBindlessIndex = 0;

		PostProcessSettings PostProcess;

		AssetHandle ActiveEnvironmentHandle = 0;
		std::unordered_map<AssetHandle, Ref<Environment>> EnvironmentCacheMap; // one bake per environment per session
		Ref<Environment> ActiveEnvironment;
		uint32_t SkyboxCubeIndex = 0;

		Ref<MeshSource> CubeMesh;
		Ref<MeshSource> SphereMesh;
		Ref<MeshSource> PlaneMesh;
		Ref<Material> DefaultMaterial;

		Ref<Shader> LitShader;
		Ref<RHI::GraphicsPipeline> ForwardOpaquePipeline;
		Ref<RHI::GraphicsPipeline> ForwardBlendedPipeline;

		Ref<Shader> SkyboxShader;
		Ref<RHI::GraphicsPipeline> SkyboxPipeline;

		std::vector<MeshSubmission> Submissions;
		std::vector<uint32_t> MaterialSlots;

		// Indices into Submissions, rebuilt every frame. Submissions itself is never reordered:
		// TransformIndex and MaterialSlots are both positional over it
		std::vector<uint32_t> OpaqueOrder;
		std::vector<uint32_t> BlendedOrder;

		PassUniforms Pass;
		uint32_t PassSlice = 0;
		uint64_t TransformsAddress = 0;
		uint64_t MaterialsAddress = 0;
		glm::mat4 Projection{1.0f};

		bool ArrayOverflowWarned = false;

		Ref<RHI::Texture> FlatNormalTexture;
		uint32_t FlatNormalIndex = 0;

		std::unordered_map<const Material*, uint32_t> MaterialIndices; // cleared every frame
		std::vector<GpuMaterial> Materials; // cleared every frame

		bool NoTargetWarned = false;

		ShadowSettings Shadows;

		Ref<RHI::RenderTarget> SunShadowTarget;
		uint32_t SunShadowLayers = 0;
		uint32_t SunShadowResolution = 0;

		Ref<Shader> ShadowDepthShader;
		std::unordered_map<uint32_t, Ref<RHI::GraphicsPipeline>> ShadowPipelines; // keyed by (ViewMask, CullMode)
		RHI::GraphicsPipeline* SunShadowPipeline = nullptr;

		bool SunCastsShadow = false;
		glm::vec3 SunDirection{0.0f, -1.0f, 0.0f};
		float SunShadowSoftness = 0.5f;
		float SunDepthBias = 1.0f;

		static constexpr float SHADOW_DEPTH_EXTENT = 6.0f;

		Renderer3D::Statistics Stats;
	};
	static Renderer3DData s_Data;

	static constexpr RHI::Format kHDRFormat = RHI::Format::RGBA16F;
	static constexpr RHI::Format kIDFormat = RHI::Format::R32I;
	static constexpr RHI::Format kDepthFormat = RHI::Format::D32F;
	static constexpr RHI::Format kLDRFormat = RHI::Format::RGBA8Unorm;
	static constexpr float kShadowConstantBias = 1.25f;
	static constexpr float kShadowSlopeBias = 1.75f;

	static glm::vec3 SRGBToLinear(const glm::vec3& c)
	{
		glm::vec3 lo = c / 12.92f;
		glm::vec3 hi = glm::pow((c + 0.055f) / 1.055f, glm::vec3(2.4f));
		return glm::vec3(c.x <= 0.04045f ? lo.x : hi.x, c.y <= 0.04045f ? lo.y : hi.y, c.z <= 0.04045f ? lo.z : hi.z);
	}

	// Bounding-sphere fit: rotation-invariant size + origin snapped to the texel grid -> no shimmer.
	static glm::mat4 FitOrthoToCorners(const glm::vec3 corners[8], const glm::vec3& lightDir, uint32_t resolution, float& outRadius)
	{
		glm::vec3 center(0.0f);
		for (int i = 0; i < 8; ++i)
			center += corners[i];
		center /= 8.0f;

		float radius = 0.0f;
		for (int i = 0; i < 8; ++i)
			radius = glm::max(radius, glm::length(corners[i] - center));
		outRadius = radius;

		glm::vec3 dir = glm::normalize(lightDir);
		glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		// Snap the sphere center to whole-texel increments in light space.
		float texelsPerUnit = float(resolution) / (2.0f * radius);
		glm::mat4 toTexel = glm::scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit)) * glm::lookAt(glm::vec3(0.0f), dir, up);
		glm::mat4 toTexelInv = glm::inverse(toTexel);

		glm::vec4 c = toTexel * glm::vec4(center, 1.0f);
		c.x = glm::floor(c.x);
		c.y = glm::floor(c.y);
		center = glm::vec3(toTexelInv * c);

		float depthExtent = radius * Renderer3DData::SHADOW_DEPTH_EXTENT;
		glm::vec3 eye = center - dir * (radius * 2.0f);
		glm::mat4 lightView = glm::lookAt(eye, center, up);
		glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, -depthExtent, depthExtent);
		return lightProj * lightView;
	}

	static void ComputeCascades(const glm::mat4& cameraViewProjection, const glm::mat4& cameraView, const glm::vec3& lightDir,
		const ShadowSettings& settings, PassUniforms& out)
	{
		const uint32_t cascadeCount = settings.CascadeCount;
		const float maxShadowDistance = settings.MaxShadowDistance;
		glm::mat4 invVP = glm::inverse(cameraViewProjection);

		// Loop order puts near corners at even indices, far corners at odd, paired as edge k = (2k, 2k+1).
		glm::vec3 corners[8];
		int idx = 0;
		for (int x = 0; x < 2; ++x)
			for (int y = 0; y < 2; ++y)
				for (int z = 0; z < 2; ++z)
				{
					glm::vec4 pt = invVP * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
					corners[idx++] = glm::vec3(pt) / pt.w;
				}

		// View looks down -z, so negate to get positive camera-space depth.
		float camNear = -(cameraView * glm::vec4(corners[0], 1.0f)).z;
		float camFar = -(cameraView * glm::vec4(corners[1], 1.0f)).z;
		float shadowFar = glm::min(camFar, maxShadowDistance);

		// Practical split: lerp of uniform and logarithmic distributions.
		float splits[MAX_CASCADES];
		for (uint32_t c = 0; c < cascadeCount; ++c)
		{
			float p = float(c + 1) / float(cascadeCount);
			float logD = camNear * glm::pow(shadowFar / camNear, p);
			float uniD = camNear + (shadowFar - camNear) * p;
			splits[c] = glm::mix(uniD, logD, settings.SplitLambda);
		}

		float range = camFar - camNear;
		float dNear = camNear;
		for (uint32_t c = 0; c < cascadeCount; ++c)
		{
			float dFar = splits[c];
			float tN = (dNear - camNear) / range;
			float tF = (dFar - camNear) / range;

			glm::vec3 slice[8];
			for (int k = 0; k < 4; ++k)
			{
				const glm::vec3& nearC = corners[2 * k];
				const glm::vec3& farC = corners[2 * k + 1];
				glm::vec3 edge = farC - nearC;
				slice[k] = nearC + edge * tN;
				slice[k + 4] = nearC + edge * tF;
			}

			float radius = 0.0f;
			out.LightViewProj[c] = FitOrthoToCorners(slice, lightDir, settings.ShadowMapResolution, radius);
			out.CascadeTexelWorld[c] = (2.0f * radius) / float(settings.ShadowMapResolution);
			out.CascadeDepthRange[c] = 2.0f * Renderer3DData::SHADOW_DEPTH_EXTENT * radius;
			out.CascadeSplits[c] = dFar;
			dNear = dFar;
		}

		out.CascadeCount = cascadeCount;
		out.VisualizeCascades = settings.VisualizeCascades ? 1u : 0u;
	}

	static glm::mat4 ComputeSpotMatrix(const glm::vec3& position, const glm::vec3& direction, float range, float outerCutoffDegrees)
	{
		glm::vec3 dir = glm::normalize(direction);
		glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 proj = glm::perspective(glm::radians(2.0f * outerCutoffDegrees), 1.0f, 0.05f, glm::max(range, 0.1f));
		glm::mat4 view = glm::lookAt(position, position + dir, up);
		return proj * view;
	}

	static const glm::vec3 s_CubeFaceDir[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
	static const glm::vec3 s_CubeFaceUp[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

	static bool EnsureHDRTarget()
	{
		const uint32_t width = s_Data.LDRTarget->GetWidth();
		const uint32_t height = s_Data.LDRTarget->GetHeight();

		if (s_Data.HDRTarget)
			s_Data.HDRTarget->Resize(width, height);
		else
			s_Data.HDRTarget = RHI::RenderTarget::Create({.Width = width,
				.Height = height,
				.ColorFormat = {kHDRFormat, kIDFormat},
				.ColorCount = 2,
				.ColorUsage = {RHI::TextureUsage::None, RHI::TextureUsage::TransferSrc},
				.DepthFormat = kDepthFormat,
				.DebugName = "HDRTarget"});

		return s_Data.HDRTarget && s_Data.HDRTarget->IsValid();
	}

	static bool EnsureSunShadowTarget()
	{
		const uint32_t resolution = s_Data.Shadows.ShadowMapResolution;
		const uint32_t layers = s_Data.Shadows.CascadeCount;

		const bool matches = s_Data.SunShadowTarget && s_Data.SunShadowTarget->IsValid() && s_Data.SunShadowResolution == resolution
			&& s_Data.SunShadowLayers == layers;

		if (!matches)
		{
			s_Data.SunShadowTarget = RHI::RenderTarget::Create({.Width = resolution,
				.Height = resolution,
				.ColorCount = 0,
				.DepthFormat = kDepthFormat,
				.Dim = RHI::Dimension::Tex2DArray,
				.ArrayLayers = layers,
				.DebugName = "SunShadowTarget"});

			s_Data.SunShadowResolution = resolution;
			s_Data.SunShadowLayers = layers;

			if (!s_Data.SunShadowTarget || !s_Data.SunShadowTarget->IsValid())
			{
				TF_CORE_ERROR("Sun shadow target ({0}x{0}, {1} layers) failed to allocate", resolution, layers);
				s_Data.SunShadowTarget = nullptr;
				s_Data.SunShadowLayers = 0;
				s_Data.SunShadowResolution = 0;
				return false;
			}

			TF_CORE_INFO("Sun shadow target: {0}x{0}, {1} cascade layers", resolution, layers);
		}

		s_Data.Pass.SunShadowIndex = s_Data.SunShadowTarget->GetDepth()->GetBindlessIndex();
		s_Data.Pass.SunShadowTexel = 1.0f / float(resolution);

		return true;
	}

	static constexpr RHI::CullMode ToRHICull(ShadowCullMode mode)
	{
		switch (mode)
		{
			case ShadowCullMode::Front: return RHI::CullMode::Front;
			case ShadowCullMode::None: return RHI::CullMode::None;
			default: return RHI::CullMode::Back;
		}
	}

	static RHI::GraphicsPipeline* GetShadowPipeline(uint32_t viewCount, ShadowCullMode cull)
	{
		if (!s_Data.ShadowDepthShader)
			return nullptr;

		const uint32_t key = (viewCount << 8) | (uint32_t)cull;

		auto it = s_Data.ShadowPipelines.find(key);
		if (it == s_Data.ShadowPipelines.end())
		{
			RHI::GraphicsPipelineDesc desc;
			desc.ShaderModule = s_Data.ShadowDepthShader;
			desc.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
				{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
			desc.Primitive = RHI::Topology::TriangleList;
			desc.ColorCount = 0;
			desc.DepthFormat = kDepthFormat;
			desc.ViewCount = viewCount;
			desc.Depth = {.Test = true, .Write = true, .Compare = RHI::CompareOp::Less, .BiasEnable = true};
			desc.Blend = RHI::BlendMode::None;
			desc.Raster.Cull = ToRHICull(cull);
			desc.DebugName = "Renderer3DShadowDepthPipeline";

			it = s_Data.ShadowPipelines.emplace(key, RHI::GraphicsPipeline::Create(desc)).first;
		}

		return it->second && it->second->IsValid() ? it->second.get() : nullptr;
	}

	static uint32_t MapIndex(AssetHandle handle, bool srgb, uint32_t fallback)
	{
		if (handle == 0 || !AssetManager::IsAssetHandleValid(handle))
			return fallback;

		Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(handle);
		if (!texture || !texture->GetRHITexture() || !texture->GetRHITexture()->IsValid())
			return fallback;

		return texture->GetRHITexture()->GetBindlessIndex(srgb);
	}

	static GpuMaterial BuildGpuMaterial(const Material& m)
	{
		const uint32_t white = RHI::Bindings::GetWhiteTextureIndex();
		const uint32_t flat = s_Data.FlatNormalIndex;

		return {.BaseColorMetallic = glm::vec4(SRGBToLinear(m.BaseColor), m.Metallic),
			.EmissiveRoughness = glm::vec4(SRGBToLinear(m.Emissive), m.Roughness),
			.Params = glm::vec4(m.NormalStrength, m.EmissiveIntensity, m.Opacity, m.AlphaCutoff),
			.Maps0 = {MapIndex(m.BaseColorMap, true, white), MapIndex(m.NormalMap, false, flat), MapIndex(m.MetallicMap, false, white),
				MapIndex(m.RoughnessMap, false, white)},
			.Maps1 = {MapIndex(m.AOMap, false, white), MapIndex(m.EmissiveMap, true, white), (uint32_t)m.Alpha, 0u}};
	}

	static std::filesystem::path ResolveEnvironmentSourcePath(AssetHandle handle)
	{
		const Ref<Project>& project = Project::GetActive();
		if (!project)
			return {};

		const Ref<EditorAssetManager> editor = std::dynamic_pointer_cast<EditorAssetManager>(project->GetAssetManager());
		if (!editor)
			return {};

		return Project::GetAssetFileSystemPath(editor->GetFilePath(handle));
	}

	static bool Prepare()
	{
		TF_PROFILE_FUNCTION();

		if (!s_Data.LDRTarget || !s_Data.LDRTarget->IsValid())
		{
			if (!s_Data.NoTargetWarned)
			{
				TF_CORE_ERROR("Renderer3D::EndScene with no render target - call SetTargetRenderTarget first");
				s_Data.NoTargetWarned = true;
			}

			return false;
		}

		if (!s_Data.ResolvePipeline || !s_Data.ResolvePipeline->IsValid())
			return false;

		if (!EnsureHDRTarget())
			return false;

		s_Data.HDRColorBindlessIndex = s_Data.HDRTarget->GetColor(0)->GetBindlessIndex(false);

		s_Data.ActiveEnvironment = nullptr;
		if (s_Data.ActiveEnvironmentHandle != 0 && AssetManager::IsAssetHandleValid(s_Data.ActiveEnvironmentHandle))
		{
			auto it = s_Data.EnvironmentCacheMap.find(s_Data.ActiveEnvironmentHandle);
			if (it == s_Data.EnvironmentCacheMap.end())
			{
				Ref<Texture2D> equirect = AssetManager::GetAsset<Texture2D>(s_Data.ActiveEnvironmentHandle);
				const std::filesystem::path sourcePath = ResolveEnvironmentSourcePath(s_Data.ActiveEnvironmentHandle);

				it = s_Data.EnvironmentCacheMap
						 .emplace(s_Data.ActiveEnvironmentHandle, equirect ? Environment::Create(equirect, sourcePath) : nullptr)
						 .first;
			}

			if (it->second && it->second->IsValid())
				s_Data.ActiveEnvironment = it->second;
		}

		s_Data.SkyboxCubeIndex = 0;
		s_Data.Pass.HasEnvironment = 0;
		s_Data.Pass.IrradianceIndex = 0;
		s_Data.Pass.PrefilterIndex = 0;
		s_Data.Pass.SkyboxIndex = 0;

		if (s_Data.ActiveEnvironment)
		{
			s_Data.SkyboxCubeIndex = s_Data.ActiveEnvironment->GetSkyboxMap()->GetBindlessIndex(false);
			s_Data.Pass.IrradianceIndex = s_Data.ActiveEnvironment->GetIrradianceMap()->GetBindlessIndex(false);
			s_Data.Pass.PrefilterIndex = s_Data.ActiveEnvironment->GetPrefilterMap()->GetBindlessIndex(false);
			s_Data.Pass.SkyboxIndex = s_Data.SkyboxCubeIndex;
			s_Data.Pass.HasEnvironment = 1;
		}

		s_Data.Pass.CascadeBlend = s_Data.Shadows.CascadeBlend;
		s_Data.Pass.BlockerSamples = (int32_t)s_Data.Shadows.BlockerSearchSamples;
		s_Data.Pass.PCFSamples = (int32_t)s_Data.Shadows.PCFSamples;
		s_Data.Pass.SoftShadows = s_Data.Shadows.SoftShadows ? 1 : 0;

		s_Data.SunShadowPipeline = nullptr;
		if (s_Data.SunCastsShadow && EnsureSunShadowTarget())
		{
			ComputeCascades(s_Data.Pass.ViewProjection, s_Data.Pass.View, s_Data.SunDirection, s_Data.Shadows, s_Data.Pass);
			// Softness is the tangent of the light's angular radius: the real sun is 0.0047 (0.27 deg),
			// so the 0..1 slider spans roughly 1x to 8x the sun and the 0.5 default sits near 4x
			s_Data.Pass.LightSize = s_Data.SunShadowSoftness * 0.04f;
			s_Data.Pass.DepthBias = s_Data.SunDepthBias;

			s_Data.Stats.ShadowCasters++;
			s_Data.Stats.CascadeCount = s_Data.Shadows.CascadeCount;

			s_Data.SunShadowPipeline = GetShadowPipeline(s_Data.Shadows.CascadeCount, s_Data.Shadows.CullMode);
		}

		s_Data.PassSlice = RHI::Bindings::WritePassUniforms(&s_Data.Pass, sizeof(s_Data.Pass));
		if (s_Data.PassSlice == UINT32_MAX)
			return false;

		s_Data.TransformsAddress = 0;
		s_Data.MaterialsAddress = 0;

		if (!s_Data.Submissions.empty())
		{
			const RHI::FrameAllocation transforms = RHI::FrameAllocator::Allocate(s_Data.Submissions.size() * sizeof(GpuTransform), 16);

			if (transforms.IsValid())
			{
				GpuTransform* out = (GpuTransform*)transforms.Mapped;
				for (size_t i = 0; i < s_Data.Submissions.size(); i++)
				{
					const glm::mat4& model = s_Data.Submissions[i].Transform;
					const glm::mat3 normal = glm::transpose(glm::inverse(glm::mat3(model)));
					out[i] = {model, glm::vec4(normal[0], 0.0f), glm::vec4(normal[1], 0.0f), glm::vec4(normal[2], 0.0f)};
				}

				s_Data.TransformsAddress = transforms.Buffer->GetDeviceAddress() + transforms.Offset;
			}
			else if (!s_Data.ArrayOverflowWarned)
			{
				TF_CORE_ERROR("Transform array did not fit the frame allocator; dropping {0} draws", s_Data.Submissions.size());
				s_Data.ArrayOverflowWarned = true;
			}

			s_Data.MaterialIndices.clear();
			s_Data.Materials.clear();
			s_Data.MaterialSlots.resize(s_Data.Submissions.size());

			for (size_t i = 0; i < s_Data.Submissions.size(); i++)
			{
				const Ref<Material>& material = s_Data.Submissions[i].Material;
				auto [it, inserted] = s_Data.MaterialIndices.try_emplace(material.get(), (uint32_t)s_Data.Materials.size());
				if (inserted)
					s_Data.Materials.push_back(BuildGpuMaterial(*material));

				s_Data.MaterialSlots[i] = it->second;
			}

			const RHI::FrameAllocation materials = RHI::FrameAllocator::Allocate(s_Data.Materials.size() * sizeof(GpuMaterial), 16);
			if (materials.IsValid())
			{
				std::memcpy(materials.Mapped, s_Data.Materials.data(), s_Data.Materials.size() * sizeof(GpuMaterial));
				s_Data.MaterialsAddress = materials.Buffer->GetDeviceAddress() + materials.Offset;
			}
			else if (!s_Data.ArrayOverflowWarned)
			{
				TF_CORE_ERROR("Material array did not fit the frame allocator; dropping {0} draws", s_Data.Submissions.size());
				s_Data.ArrayOverflowWarned = true;
			}

			s_Data.Stats.MaterialBinds = (uint32_t)s_Data.Materials.size();
		}

		s_Data.OpaqueOrder.clear();
		s_Data.BlendedOrder.clear();
		s_Data.OpaqueOrder.reserve(s_Data.Submissions.size());

		for (uint32_t i = 0; i < (uint32_t)s_Data.Submissions.size(); i++)
			if (s_Data.Submissions[i].Material->Alpha == AlphaMode::Blend)
				s_Data.BlendedOrder.push_back(i);
			else
				s_Data.OpaqueOrder.push_back(i);

		const glm::vec3 cameraPosition = glm::vec3(s_Data.Pass.CameraPosition);
		std::ranges::sort(s_Data.BlendedOrder, [&](uint32_t a, uint32_t b) {
			const glm::vec3 da = glm::vec3(s_Data.Submissions[a].Transform[3]) - cameraPosition;
			const glm::vec3 db = glm::vec3(s_Data.Submissions[b].Transform[3]) - cameraPosition;
			return glm::dot(da, da) > glm::dot(db, db);
		});

		return true;
	}

	static void Record()
	{
		TF_PROFILE_FUNCTION();

		RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList();
		if (!cmd)
			return;

		if (s_Data.Pass.CascadeCount > 0 && s_Data.SunShadowTarget && s_Data.SunShadowTarget->IsValid() && s_Data.TransformsAddress != 0)
		{
			if (RHI::GraphicsPipeline* pipeline = s_Data.SunShadowPipeline)
			{
				TF_PROFILE_SCOPE("Shadow Sun");
				TF_PROFILE_GPU_SCOPE("Shadow Sun");
				PerformanceStats::ScopedPassTimer passTimer("Shadow Sun");

				cmd->BeginPass({.DebugName = "Shadow Sun",
					.Target = s_Data.SunShadowTarget.get(),
					.ViewCount = s_Data.Shadows.CascadeCount,
					.Depth = {.Load = RHI::LoadOp::Clear, .ClearDepth = 1.0f}});

				cmd->SetPassUniformSlice(s_Data.PassSlice);
				cmd->BindPipeline(*pipeline);
				cmd->SetDepthBias(kShadowConstantBias * s_Data.SunDepthBias, kShadowSlopeBias * s_Data.SunDepthBias);

				const MeshSource* boundMesh = nullptr;
				for (uint32_t i = 0; i < (uint32_t)s_Data.Submissions.size(); i++)
				{
					const MeshSubmission& sub = s_Data.Submissions[i];
					if (sub.Mesh.get() != boundMesh)
					{
						cmd->BindVertexBuffer(*sub.Mesh->GetVertexBuffer());
						cmd->BindIndexBuffer(*sub.Mesh->GetIndexBuffer(), RHI::IndexType::U32);
					}

					const ShadowPush push{.Transforms = s_Data.TransformsAddress, .TransformIndex = i};
					cmd->PushConstants(&push, sizeof(push));

					const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
					cmd->DrawIndexed(sm.IndexCount, 1, sm.BaseIndex, (int32_t)sm.BaseVertex);

					s_Data.Stats.DrawCalls++;
					s_Data.Stats.ShadowDrawCalls++;
				}

				cmd->EndPass();
			}
		}

		{
			TF_PROFILE_SCOPE("Forward Opaque");
			TF_PROFILE_GPU_SCOPE("Forward Opaque");
			PerformanceStats::ScopedPassTimer passTimer("Forward Opaque");

			cmd->BeginPass({.DebugName = "Forward Opaque",
				.Target = s_Data.HDRTarget.get(),
				.Color = {{.Load = RHI::LoadOp::Clear, .ClearValue = {0.0f, 0.0f, 0.0f, 1.0f}},
					{.Load = RHI::LoadOp::Clear, .ClearInt = {-1, -1, -1, 0}}},
				.Depth = {.Load = RHI::LoadOp::Clear, .ClearDepth = 1.0f}});

			cmd->SetPassUniformSlice(s_Data.PassSlice);

			if (s_Data.TransformsAddress != 0 && s_Data.MaterialsAddress != 0 && s_Data.ForwardOpaquePipeline
				&& s_Data.ForwardOpaquePipeline->IsValid())
			{
				cmd->BindPipeline(*s_Data.ForwardOpaquePipeline);

				const MeshSource* boundMesh = nullptr;
				for (uint32_t i : s_Data.OpaqueOrder)
				{
					const MeshSubmission& sub = s_Data.Submissions[i];

					if (sub.Mesh.get() != boundMesh)
					{
						cmd->BindVertexBuffer(*sub.Mesh->GetVertexBuffer());
						cmd->BindIndexBuffer(*sub.Mesh->GetIndexBuffer(), RHI::IndexType::U32);
						boundMesh = sub.Mesh.get();
					}

					const DrawPush push{.Transforms = s_Data.TransformsAddress,
						.Materials = s_Data.MaterialsAddress,
						.TransformIndex = i,
						.MaterialIndex = i < s_Data.MaterialSlots.size() ? s_Data.MaterialSlots[i] : 0u,
						.EntityID = sub.EntityID};

					cmd->PushConstants(&push, sizeof(push));

					const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
					cmd->DrawIndexed(sm.IndexCount, 1, sm.BaseIndex, (int32_t)sm.BaseVertex);

					s_Data.Stats.DrawCalls++;
					s_Data.Stats.OpaqueMeshes++;
					s_Data.Stats.IndexCount += sm.IndexCount;
					s_Data.Stats.TriangleCount += sm.IndexCount / 3;
				}
			}

			cmd->EndPass();
		}

		if (s_Data.ActiveEnvironment && s_Data.SkyboxPipeline && s_Data.SkyboxPipeline->IsValid() && s_Data.CubeMesh
			&& s_Data.CubeMesh->HasGpuBuffers())
		{
			TF_PROFILE_SCOPE("Skybox");
			TF_PROFILE_GPU_SCOPE("Skybox");
			PerformanceStats::ScopedPassTimer passTimer("Skybox");

			cmd->BeginPass({.DebugName = "Skybox",
				.Target = s_Data.HDRTarget.get(),
				.Color = {{.Load = RHI::LoadOp::Load}, {.Load = RHI::LoadOp::Load}},
				.Depth = {.Load = RHI::LoadOp::Load}});

			cmd->SetPassUniformSlice(s_Data.PassSlice);
			cmd->BindPipeline(*s_Data.SkyboxPipeline);
			cmd->BindVertexBuffer(*s_Data.CubeMesh->GetVertexBuffer());
			cmd->BindIndexBuffer(*s_Data.CubeMesh->GetIndexBuffer(), RHI::IndexType::U32);

			const SkyboxPush push{.SkyViewProjection = s_Data.Projection * glm::mat4(glm::mat3(s_Data.Pass.View)),
				.SkyboxIndex = s_Data.SkyboxCubeIndex,
				.EnvRotation = s_Data.Pass.EnvRotation};

			cmd->PushConstants(&push, sizeof(push));

			const Submesh& sky = s_Data.CubeMesh->GetSubmeshes()[0];
			cmd->DrawIndexed(sky.IndexCount, 1, sky.BaseIndex, (int32_t)sky.BaseVertex);
			s_Data.Stats.DrawCalls++;

			cmd->EndPass();
		}

		if (!s_Data.BlendedOrder.empty() && s_Data.TransformsAddress != 0 && s_Data.MaterialsAddress != 0 && s_Data.ForwardBlendedPipeline
			&& s_Data.ForwardBlendedPipeline->IsValid())
		{
			TF_PROFILE_SCOPE("Forward Blended");
			TF_PROFILE_GPU_SCOPE("Forward Blended");
			PerformanceStats::ScopedPassTimer passTimer("Forward Blended");

			cmd->BeginPass({.DebugName = "Forward Blended",
				.Target = s_Data.HDRTarget.get(),
				.Color = {{.Load = RHI::LoadOp::Load}, {.Load = RHI::LoadOp::Load}},
				.Depth = {.Load = RHI::LoadOp::Load}});

			cmd->SetPassUniformSlice(s_Data.PassSlice);
			cmd->BindPipeline(*s_Data.ForwardBlendedPipeline);

			const MeshSource* boundMesh = nullptr;
			for (uint32_t i : s_Data.BlendedOrder)
			{
				const MeshSubmission& sub = s_Data.Submissions[i];

				if (sub.Mesh.get() != boundMesh)
				{
					cmd->BindVertexBuffer(*sub.Mesh->GetVertexBuffer());
					cmd->BindIndexBuffer(*sub.Mesh->GetIndexBuffer(), RHI::IndexType::U32);
					boundMesh = sub.Mesh.get();
				}

				const DrawPush push{.Transforms = s_Data.TransformsAddress,
					.Materials = s_Data.MaterialsAddress,
					.TransformIndex = i,
					.MaterialIndex = i < s_Data.MaterialSlots.size() ? s_Data.MaterialSlots[i] : 0u,
					.EntityID = sub.EntityID};

				cmd->PushConstants(&push, sizeof(push));

				const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
				cmd->DrawIndexed(sm.IndexCount, 1, sm.BaseIndex, (int32_t)sm.BaseVertex);

				s_Data.Stats.DrawCalls++;
				s_Data.Stats.BlendedMeshes++;
				s_Data.Stats.IndexCount += sm.IndexCount;
				s_Data.Stats.TriangleCount += sm.IndexCount / 3;
			}

			cmd->EndPass();
		}

		cmd->CopyTexture(*s_Data.HDRTarget->GetColor(1), *s_Data.LDRTarget->GetColor(1));

		{
			TF_PROFILE_SCOPE("HDR Resolve");
			TF_PROFILE_GPU_SCOPE("HDR Resolve");
			PerformanceStats::ScopedPassTimer passTimer("HDR Resolve");

			cmd->BeginPass({.DebugName = "HDR Resolve",
				.Target = s_Data.LDRTarget.get(),
				.Color = {{.Load = RHI::LoadOp::DontCare}, {.Load = RHI::LoadOp::Load}}});

			cmd->SetPassUniformSlice(s_Data.PassSlice);

			cmd->BindPipeline(*s_Data.ResolvePipeline);

			const ResolvePush push{.HDRColorIndex = s_Data.HDRColorBindlessIndex,
				.ExposureEV = s_Data.PostProcess.ExposureEV,
				.Operator = (int32_t)s_Data.PostProcess.Operator,
				.WhitePoint = s_Data.PostProcess.ReinhardWhitePoint};

			cmd->PushConstants(&push, sizeof(push));
			cmd->Draw(3);
			s_Data.Stats.DrawCalls++;

			cmd->EndPass();
		}
	}

	void Renderer3D::Init()
	{
		TF_PROFILE_FUNCTION();

		{
			RHI::UploadScope upload;
			s_Data.CubeMesh = MeshSource::CreateCube();
			s_Data.SphereMesh = MeshSource::CreateSphere();
			s_Data.PlaneMesh = MeshSource::CreatePlane();
		}

		s_Data.DefaultMaterial = CreateRef<Material>();

		constexpr uint32_t flatNormal = 0xffff8080; // RGBA8 LE -> (128, 128, 255, 255) = tangent-space (0, 0, 1)
		s_Data.FlatNormalTexture = RHI::Texture::CreateWithData(
			{.Width = 1, .Height = 1, .PixelFormat = RHI::Format::RGBA8Unorm, .MipLevels = 1, .SRGBView = false, .DebugName = "FlatNormal"},
			&flatNormal, sizeof(flatNormal));

		s_Data.FlatNormalIndex = (s_Data.FlatNormalTexture && s_Data.FlatNormalTexture->IsValid())
			? s_Data.FlatNormalTexture->GetBindlessIndex(false)
			: RHI::Bindings::GetWhiteTextureIndex();

		s_Data.ResolveShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_HDRResolve.slang");

		RHI::GraphicsPipelineDesc hdr;
		hdr.ShaderModule = s_Data.ResolveShader;
		hdr.Primitive = RHI::Topology::TriangleList;
		hdr.ColorFormats[0] = kLDRFormat;
		hdr.ColorFormats[1] = kIDFormat;
		hdr.ColorCount = 2;
		hdr.ColorWrite[1] = false; // the ids were copied in already; the resolve must not touch them
		hdr.DepthFormat = RHI::Format::Undefined;
		hdr.Depth = {.Test = false, .Write = false};
		hdr.Blend = RHI::BlendMode::None;
		hdr.Raster.Cull = RHI::CullMode::None;
		hdr.DebugName = "Renderer3DResolvePipeline";

		s_Data.ResolvePipeline = RHI::GraphicsPipeline::Create(hdr);

		s_Data.LitShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_Lit.slang");

		RHI::GraphicsPipelineDesc lit;
		lit.ShaderModule = s_Data.LitShader;
		lit.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
			{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
		lit.Primitive = RHI::Topology::TriangleList;
		lit.ColorFormats[0] = kHDRFormat;
		lit.ColorFormats[1] = kIDFormat;
		lit.ColorCount = 2;
		lit.DepthFormat = kDepthFormat;
		lit.Depth = {.Test = true, .Write = true, .Compare = RHI::CompareOp::Less};
		lit.Blend = RHI::BlendMode::None;
		lit.Raster.Cull = RHI::CullMode::Back;
		lit.DebugName = "Renderer3DForwardOpaquePipeline";

		s_Data.ForwardOpaquePipeline = RHI::GraphicsPipeline::Create(lit);

		RHI::GraphicsPipelineDesc blended = lit;
		blended.Depth = {.Test = true, .Write = false, .Compare = RHI::CompareOp::Less};
		blended.Blend = RHI::BlendMode::Alpha;
		blended.DebugName = "Renderer3DForwardBlendedPipeline";

		s_Data.ForwardBlendedPipeline = RHI::GraphicsPipeline::Create(blended);

		s_Data.ShadowDepthShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_ShadowDepth.slang");

		s_Data.SkyboxShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_Skybox.slang");

		RHI::GraphicsPipelineDesc sky;
		sky.ShaderModule = s_Data.SkyboxShader;
		sky.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Normal"},
			{ShaderDataType::Float2, "a_TexCoord"}, {ShaderDataType::Float3, "a_Tangent"}, {ShaderDataType::Float3, "a_Bitangent"}};
		sky.Primitive = RHI::Topology::TriangleList;
		sky.ColorFormats[0] = kHDRFormat;
		sky.ColorFormats[1] = kIDFormat;
		sky.ColorCount = 2;
		sky.DepthFormat = kDepthFormat;
		sky.Depth = {.Test = true, .Write = false, .Compare = RHI::CompareOp::LessOrEqual};
		sky.Blend = RHI::BlendMode::None;
		sky.Raster.Cull = RHI::CullMode::Front;
		sky.DebugName = "Renderer3DSkyboxPipeline";

		s_Data.SkyboxPipeline = RHI::GraphicsPipeline::Create(sky);
	}

	void Renderer3D::Shutdown()
	{
		s_Data.MaterialIndices.clear();
		s_Data.Materials.clear();
		s_Data.Submissions.clear();
		s_Data = {}; // every Ref it holds owns a GPU resource that must die before the device

		Environment::ReleaseResources();
	}

	void Renderer3D::SetTargetRenderTarget(const Ref<RHI::RenderTarget>& target)
	{
		s_Data.LDRTarget = target;
	}

	void Renderer3D::BeginScene(const EditorCamera& camera)
	{
		ResetStats();
		s_Data.Submissions.clear();
		s_Data.Pass = {};
		s_Data.ActiveEnvironmentHandle = 0; // outside Pass, so reset it here: no SkyLight means no environment
		s_Data.SunCastsShadow = false;
		s_Data.Pass.ViewProjection = camera.GetViewProjection();
		s_Data.Pass.View = camera.GetView();
		s_Data.Pass.CameraPosition = glm::vec4(camera.GetPosition(), 1.0f);
		s_Data.Projection = camera.GetProjection();
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		ResetStats();
		s_Data.Submissions.clear();
		s_Data.Pass = {};
		s_Data.ActiveEnvironmentHandle = 0; // outside Pass, so reset it here: no SkyLight means no environment
		s_Data.SunCastsShadow = false;
		const glm::mat4 view = glm::inverse(transform);
		s_Data.Pass.ViewProjection = camera.GetProjection() * view;
		s_Data.Pass.View = view;
		s_Data.Pass.CameraPosition = glm::vec4(glm::vec3(transform[3]), 1.0f);
		s_Data.Projection = camera.GetProjection();
	}

	void Renderer3D::SetShadowSettings(const ShadowSettings& settings)
	{
		s_Data.Shadows = settings;
		s_Data.Shadows.CascadeCount = glm::clamp(s_Data.Shadows.CascadeCount, 1u, ShadowSettings::MaxCascades);
		s_Data.Shadows.ShadowMapResolution = glm::max(s_Data.Shadows.ShadowMapResolution, 1u);
	}

	void Renderer3D::SetPostProcessSettings(const PostProcessSettings& settings)
	{
		s_Data.PostProcess = settings;
	}

	void Renderer3D::EndScene()
	{
		TF_PROFILE_FUNCTION();

		if (Prepare())
			Record();
	}

	void Renderer3D::SubmitMesh(
		const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex, const Ref<Material>& material, int entityID)
	{
		if (!mesh || !mesh->HasGpuBuffers() || submeshIndex >= mesh->GetSubmeshes().size())
			return;

		s_Data.Submissions.push_back({transform, mesh, submeshIndex, material ? material : s_Data.DefaultMaterial, entityID});
	}

	Renderer3D::Statistics& Renderer3D::GetStats()
	{
		return s_Data.Stats;
	}

	void Renderer3D::ResetStats()
	{
		s_Data.Stats = {};
	}

	Ref<Material> Renderer3D::GetDefaultMaterial()
	{
		return s_Data.DefaultMaterial;
	}

	void Renderer3D::RegisterBuiltInMeshes(EditorAssetManager& assetManager)
	{
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Cube, s_Data.CubeMesh, "Cube", AssetType::Mesh);
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Sphere, s_Data.SphereMesh, "Sphere", AssetType::Mesh);
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Plane, s_Data.PlaneMesh, "Plane", AssetType::Mesh);
	}

	void Renderer3D::SubmitDirectionalLight(
		const glm::vec3& direction, const glm::vec3& color, float intensity, bool castsShadows, float shadowSoftness, float depthBias)
	{
		if (s_Data.Pass.DirCount >= MAX_DIR_LIGHTS)
			return;

		GpuDirLight& light = s_Data.Pass.DirLights[s_Data.Pass.DirCount++];
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color = glm::vec4(SRGBToLinear(color), intensity);

		if (castsShadows && !s_Data.SunCastsShadow)
		{
			s_Data.SunCastsShadow = true;
			s_Data.SunDirection = glm::normalize(direction);
			s_Data.SunShadowSoftness = shadowSoftness;
			s_Data.SunDepthBias = depthBias;
		}

		s_Data.Stats.DirectionalLights = s_Data.Pass.DirCount;
	}

	void Renderer3D::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range, bool castsShadows,
		float shadowSoftness, float depthBias)
	{
		if (s_Data.Pass.PointCount >= MAX_POINT_LIGHTS)
			return;

		GpuPointLight& light = s_Data.Pass.PointLights[s_Data.Pass.PointCount++];
		light.Position = glm::vec4(position, range);
		light.Color = glm::vec4(SRGBToLinear(color), intensity);

		s_Data.Stats.PointLights = s_Data.Pass.PointCount;
	}

	void Renderer3D::SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity,
		float range, float innerCutoffDegrees, float outerCutoffDegrees, bool castsShadows, float shadowSoftness, float depthBias)
	{
		if (s_Data.Pass.SpotCount >= MAX_SPOT_LIGHTS)
			return;

		const float innerCos = glm::cos(glm::radians(innerCutoffDegrees));
		const float outerCos = glm::cos(glm::radians(outerCutoffDegrees));

		GpuSpotLight& light = s_Data.Pass.SpotLights[s_Data.Pass.SpotCount++];
		light.Position = glm::vec4(position, 0.0f);
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color = glm::vec4(SRGBToLinear(color), 0.0f);
		light.Params = glm::vec4(range, innerCos, outerCos, intensity);

		s_Data.Stats.SpotLights = s_Data.Pass.SpotCount;
	}

	void Renderer3D::SubmitEnvironment(AssetHandle environmentMap, float intensity, float rotationDegrees)
	{
		s_Data.ActiveEnvironmentHandle = environmentMap;
		s_Data.Pass.EnvIntensity = intensity;
		s_Data.Pass.EnvRotation = glm::radians(rotationDegrees);
	}
}
