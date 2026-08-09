#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Framebuffer.h"
#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/ShadowMap.h"
#include "Timefall/Renderer/CubeShadowMap.h"
#include "Timefall/Renderer/Environment.h"
#include "Timefall/Renderer/GPUProfiler.h"
#include "Timefall/Debug/PerformanceStats.h"
#include "Timefall/Asset/AssetManager.h"
#include "Timefall/Asset/EditorAssetManager.h"

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

	struct Renderer3DData
	{
		Ref<MeshSource> CubeMesh;
		Ref<MeshSource> SphereMesh;
		Ref<MeshSource> PlaneMesh;

		Ref<Shader> LitShader;
		Ref<UniformBuffer> CameraUniformBuffer;
		CameraData CameraBuffer;

		Ref<UniformBuffer> LightsUniformBuffer;
		LightsData LightsBuffer;
		bool LightsDirty = true;

		Ref<UniformBuffer> MaterialUniformBuffer;
		MaterialData MaterialBuffer;

		Ref<UniformBuffer> EnvironmentUniformBuffer;
		EnvironmentData EnvironmentBuffer;

		Ref<Texture2D> WhiteTexture;
		Ref<Texture2D> FlatNormalTexture;
		Ref<Material> DefaultMaterial;

		std::vector<MeshSubmission> Submissions;

		Ref<ShadowMap> SunShadowMap;
		Ref<Shader> ShadowDepthShader;
		Ref<Shader> PointShadowDepthShader;
		Ref<UniformBuffer> ShadowUniformBuffer;
		ShadowData ShadowBuffer;
		bool SunCastsShadow = false;
		glm::vec3 SunDirection{0.0f, -1.0f, 0.0f};
		float SunShadowSoftness = 0.5f;
		float SunDepthBias = 1.0f;
		ShadowSettings Shadows;

		Ref<ShadowMap> SpotShadowMap;
		Ref<UniformBuffer> SpotShadowUniformBuffer;
		SpotShadowData SpotShadowBuffer;
		bool AnySpotCasts = false;

		Ref<CubeShadowMap> PointShadowMap;
		Ref<UniformBuffer> PointShadowUniformBuffer;
		PointShadowData PointShadowBuffer;
		std::vector<PointCaster> PointCasters;
		bool AnyPointCasts = false;

		Ref<Framebuffer> TargetFB; // external LDR target (owns id+depth we alias)
		Ref<Framebuffer> HdrFB; // internal RGBA16F scene buffer
		Ref<Shader> ResolveShader;
		Ref<Shader> SkyboxShader;
		PostProcessSettings PostProcess;

		static constexpr uint32_t BASE_COLOR_SAMPLER_SLOT = 0;
		static constexpr uint32_t METALLIC_SAMPLER_SLOT = 1;
		static constexpr uint32_t SHADOW_SAMPLER_SLOT = 2;
		static constexpr uint32_t SPOT_SHADOW_SAMPLER_SLOT = 3;
		static constexpr uint32_t POINT_SHADOW_SAMPLER_SLOT = 4;
		static constexpr uint32_t NORMAL_SAMPLER_SLOT = 5;
		static constexpr uint32_t ROUGHNESS_SAMPLER_SLOT = 6;
		static constexpr uint32_t AO_SAMPLER_SLOT = 7;
		static constexpr uint32_t EMISSIVE_SAMPLER_SLOT = 8;
		static constexpr uint32_t IRRADIANCE_SAMPLER_SLOT = 9;
		static constexpr uint32_t PREFILTER_SAMPLER_SLOT = 10;
		static constexpr uint32_t SKYBOX_SAMPLER_SLOT = 11;
		static constexpr float SHADOW_DEPTH_EXTENT = 6.0f; // ortho slab = radius * this each way along the light

		// Per-frame render-state cache (reset in BeginScene) to skip redundant GL state changes
		// across the many SubmitMesh calls of a frame.
		const Material* CurrentMaterial = nullptr;

		AssetHandle ActiveEnvironmentHandle = 0;
		float EnvIntensity = 1.0f;
		float EnvRotationRadians = 0.0f;
		std::unordered_map<AssetHandle, Ref<Environment>> EnvironmentCache;
		Ref<Environment> ActiveEnvironment;

		Renderer3D::Statistics Stats;
	};

	static Renderer3DData s_Data;

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

	static void EnsureHdrFramebuffer()
	{
		const auto& target = s_Data.TargetFB;
		if (!target)
			return;

		const FramebufferSpecification& tspec = target->GetSpecification();
		bool needsRebuild = !s_Data.HdrFB || s_Data.HdrFB->GetSpecification().Width != tspec.Width
			|| s_Data.HdrFB->GetSpecification().Height != tspec.Height;

		if (!needsRebuild)
			return;

		FramebufferSpecification spec;
		spec.Width = tspec.Width;
		spec.Height = tspec.Height;

		FramebufferTextureSpecification hdrColor(FramebufferTextureFormat::RGBA16F);

		FramebufferTextureSpecification idAlias(FramebufferTextureFormat::RED_INTEGER);
		idAlias.ExternalRendererID = target->GetColorAttachmentRendererID(1);

		FramebufferTextureSpecification depthAlias(FramebufferTextureFormat::DEPTH24STENCIL8);
		depthAlias.ExternalRendererID = target->GetDepthAttachmentRendererID();

		spec.Attachments = {hdrColor, idAlias, depthAlias};
		s_Data.HdrFB = Framebuffer::Create(spec);
	}

	void Renderer3D::Init() {}

	void Renderer3D::Shutdown() {}

	void Renderer3D::SetTargetFramebuffer(const Ref<Framebuffer>& target) {}

	void Renderer3D::BeginScene(const EditorCamera& camera) {}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform) {}

	void Renderer3D::SetShadowSettings(const ShadowSettings& settings) {}

	void Renderer3D::SetPostProcessSettings(const PostProcessSettings& settings) {}

	void Renderer3D::EndScene() {}

	void Renderer3D::SubmitMesh(
		const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex, const Ref<Material>& material, int entityID)
	{}

	Renderer3D::Statistics& Renderer3D::GetStats()
	{
		static Statistics s_Empty{};
		return s_Empty;
	}

	void Renderer3D::ResetStats() {}

	Ref<Material> Renderer3D::GetDefaultMaterial()
	{
		static Ref<Material> s_Default = CreateRef<Material>();
		return s_Default;
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
