#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/ShaderLibrary.h"
#include "Timefall/Renderer/GPUProfiler.h"
#include "Timefall/Debug/PerformanceStats.h"
#include "Timefall/Asset/AssetManager.h"
#include "Timefall/Asset/EditorAssetManager.h"

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/RHI/Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Timefall
{
	// std140 layout matching the Camera UBO block in Renderer3D_Lit.glsl.
	struct CameraData
	{
		glm::mat4 ViewProjection;
		glm::mat4 View;
		glm::vec3 CameraPosition;
		float _Padding = 0.0f;
	};

	static constexpr uint32_t MAX_DIR_LIGHTS = 4;
	static constexpr uint32_t MAX_POINT_LIGHTS = 32;
	static constexpr uint32_t MAX_SPOT_LIGHTS = 16;

	// std140 mirrors of the Lights UBO block in Renderer3D_Lit.glsl. All vec4 -> 16-byte aligned.
	struct GpuDirLight
	{
		glm::vec4 Direction;
		glm::vec4 Color;
	}; // Color.a = intensity
	struct GpuPointLight
	{
		glm::vec4 Position;
		glm::vec4 Color;
	}; // Position.w = range, Color.a = intensity
	struct GpuSpotLight
	{
		glm::vec4 Position;
		glm::vec4 Direction;
		glm::vec4 Color;
		glm::vec4 Params;
	};
	// Spot: Position.xyz = position, Direction.xyz = direction, Color.rgb = color,
	//       Params = (range, innerCos, outerCos, intensity)

	struct LightsData
	{
		GpuDirLight DirLights[MAX_DIR_LIGHTS];
		GpuPointLight PointLights[MAX_POINT_LIGHTS];
		GpuSpotLight SpotLights[MAX_SPOT_LIGHTS];
		uint32_t DirCount = 0;
		uint32_t PointCount = 0;
		uint32_t SpotCount = 0;
		uint32_t _Padding = 0;
	};

	// std140 mirror of the Material UBO block in Renderer3D_Lit.glsl.
	struct MaterialData
	{
		glm::vec3 BaseColor = glm::vec3(1.0f);
		float Metallic = 0.0f; // vec3 + float share one 16B slot
		glm::vec3 Emissive = glm::vec3(0.0f);
		float Roughness = 1.0f;
		float NormalStrength = 1.0f;
		float EmissiveIntensity = 1.0f;
		float Opacity = 1.0f;
		float AlphaCutoff = 0.5f;
		int32_t AlphaMode = 0;
		float _Pad0 = 0.0f, _Pad1 = 0.0f, _Pad2 = 0.0f;
	};

	// std140 mirror of the Environment UBO block in Renderer3D_Lit.glsl.
	struct EnvironmentData
	{
		float EnvIntensity = 1.0f;
		float EnvRotation = 0.0f;
		float MaxReflectionLod = 4.0f;
		int32_t HasEnvironment = 0;
	};

	struct MeshSubmission
	{
		glm::mat4 Transform;
		Ref<MeshSource> Mesh;
		uint32_t SubmeshIndex;
		Ref<Material> Material;
		int EntityID;
	};

	static constexpr uint32_t MAX_CASCADES = 4;

	// std140 mirror of the Shadows UBO block in Renderer3D_Lit.glsl.
	struct ShadowData
	{
		glm::mat4 LightViewProj[MAX_CASCADES];
		glm::vec4 CascadeSplits; // far view-depth of each cascade
		glm::vec4 CascadeTexelWorld; // world units per shadow texel, per cascade
		glm::vec4 CascadeDepthRange; // world depth mapped to [0,1] per cascade
		uint32_t CascadeCount = 0;
		uint32_t VisualizeCascades = 0;
		float LightSize = 0.08f; // PCSS light size (from the sun's ShadowSoftness)
		float DepthBias = 1.0f; // multiplier on the shader depth bias
		float CascadeBlend = 0.1f; // boundary blend band (fraction of cascade extent)
		int32_t BlockerSamples = 16; // PCSS blocker search taps
		int32_t PCFSamples = 16; // PCSS / fixed-PCF filter taps
		int32_t SoftShadows = 1; // 1 = PCSS, 0 = fixed-kernel PCF
	};

	// std140 mirror of the SpotShadows UBO block. Indexed by spot light index.
	struct SpotShadowData
	{
		glm::mat4 LightViewProj[MAX_SPOT_LIGHTS];
		glm::vec4 Params[MAX_SPOT_LIGHTS]; // x = casts(0/1), y = lightSize, z = depthBias, w = atlas layer
	};

	// std140 mirror of the PointShadows UBO block. Indexed by point light index.
	struct PointShadowData
	{
		glm::vec4 Params[MAX_POINT_LIGHTS]; // x = casts(0/1), y = lightSize, z = depthBias, w = cubeLayer
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

		PostProcessSettings PostProcessSettings;

		bool NoTargetWarned = false;

		static constexpr float SHADOW_DEPTH_EXTENT = 6.0f;

		Renderer3D::Statistics Stats;
	};
	static Renderer3DData s_Data;

	static constexpr RHI::Format kHDRFormat = RHI::Format::RGBA16F;
	static constexpr RHI::Format kIDFormat = RHI::Format::R32I;
	static constexpr RHI::Format kDepthFormat = RHI::Format::D32F;
	static constexpr RHI::Format kLDRFormat = RHI::Format::RGBA8Unorm;

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
		const ShadowSettings& settings, ShadowData& out)
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
		return true;
	}

	static void Record()
	{
		TF_PROFILE_FUNCTION();

		RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList();
		if (!cmd)
			return;

		{
			TF_PROFILE_SCOPE("Forward Opaque");
			TF_PROFILE_GPU_SCOPE("Forward Opaque");
			PerformanceStats::ScopedPassTimer passTimer("Forward Opaque");

			cmd->BeginPass(
				{
				.DebugName = "Forward Opaque", .Target = s_Data.HDRTarget.get(), .Color = {
					{.Load = RHI::LoadOp::Clear, .ClearValue = {0.0f, 0.0f, 0.0f, 1.0f}},
					{.Load = RHI::LoadOp::Clear, .ClearInt = {-1, -1, -1, 0}}},
				.Depth = {.Load = RHI::LoadOp::Clear, .ClearDepth = 1.0f}});

			cmd->EndPass();
		}

		cmd->CopyTexture(*s_Data.HDRTarget->GetColor(1), *s_Data.LDRTarget->GetColor(1));

		{
			TF_PROFILE_SCOPE("HDR Resolve");
			TF_PROFILE_GPU_SCOPE("HDR Resolve");
			PerformanceStats::ScopedPassTimer passTimer("HDR Resolve");

			cmd->BeginPass({.DebugName = "HDR Resolve", .Target = s_Data.LDRTarget.get(), .Color = {{.Load = RHI::LoadOp::DontCare}, {.Load = RHI::LoadOp::Load}}});

			cmd->BindPipeline(*s_Data.ResolvePipeline);

			const ResolvePush push{.HDRColorIndex = s_Data.HDRColorBindlessIndex,
				.ExposureEV = s_Data.PostProcessSettings.ExposureEV,
				.Operator = (int32_t)s_Data.PostProcessSettings.Operator,
				.WhitePoint = s_Data.PostProcessSettings.ReinhardWhitePoint};

			cmd->PushConstants(&push, sizeof(push));
			cmd->Draw(3);
			s_Data.Stats.DrawCalls++;

			cmd->EndPass();
		}
	}

	void Renderer3D::Init()
	{
		TF_PROFILE_FUNCTION();

		s_Data.ResolveShader = ShaderLibrary::Load("Assets/Shaders/Renderer3D_HDRResolve.slang");

		RHI::GraphicsPipelineDesc desc;
		desc.ShaderModule = s_Data.ResolveShader;
		desc.Primitive = RHI::Topology::TriangleList;
		desc.ColorFormats[0] = kLDRFormat;
		desc.ColorFormats[1] = kIDFormat;
		desc.ColorCount = 2;
		desc.ColorWrite[1] = false; // the ids were copied in already; the resolve must not touch them
		desc.DepthFormat = RHI::Format::Undefined;
		desc.Depth = {.Test = false, .Write = false};
		desc.Blend = RHI::BlendMode::None;
		desc.Raster.Cull = RHI::CullMode::None;
		desc.DebugName = "Renderer3DResolvePipeline";

		s_Data.ResolvePipeline = RHI::GraphicsPipeline::Create(desc);
	}

	void Renderer3D::Shutdown()
	{
		s_Data = {}; // every Ref it holds owns a GPU resource that must die before the device
	}

	void Renderer3D::SetTargetRenderTarget(const Ref<RHI::RenderTarget>& target)
	{
		s_Data.LDRTarget = target;
	}

	void Renderer3D::BeginScene(const EditorCamera& camera)
	{
		ResetStats();
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		ResetStats();
	}

	void Renderer3D::SetShadowSettings(const ShadowSettings& settings) {}

	void Renderer3D::SetPostProcessSettings(const PostProcessSettings& settings)
	{
		s_Data.PostProcessSettings = settings;
	}

	void Renderer3D::EndScene()
	{
		TF_PROFILE_FUNCTION();

		if (Prepare())
			Record();
	}

	void Renderer3D::SubmitMesh(
		const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex, const Ref<Material>& material, int entityID)
	{}

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
		//if (!s_Data.DefaultMaterial)
		//	s_Data.DefaultMaterial = CreateRef<Material>();
		//
		//return s_Data.DefaultMaterial;
		return {};
	}

	void Renderer3D::RegisterBuiltInMeshes(EditorAssetManager& assetManager) {}

	void Renderer3D::SubmitDirectionalLight(
		const glm::vec3& direction, const glm::vec3& color, float intensity, bool castsShadows, float shadowSoftness, float depthBias)
	{}

	void Renderer3D::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range, bool castsShadows,
		float shadowSoftness, float depthBias)
	{}

	void Renderer3D::SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity,
		float range, float innerCutoffDegrees, float outerCutoffDegrees, bool castsShadows, float shadowSoftness, float depthBias)
	{}

	void Renderer3D::SubmitEnvironment(AssetHandle environmentMap, float intensity, float rotationDegrees) {}
}
