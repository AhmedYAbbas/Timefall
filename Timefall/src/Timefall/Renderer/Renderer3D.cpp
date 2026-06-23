#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/RenderCommand.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/ShadowMap.h"
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

	static constexpr uint32_t MAX_DIR_LIGHTS   = 4;
	static constexpr uint32_t MAX_POINT_LIGHTS = 32;
	static constexpr uint32_t MAX_SPOT_LIGHTS  = 16;

	// std140 mirrors of the Lights UBO block in Renderer3D_Lit.glsl. All vec4 -> 16-byte aligned.
	struct GpuDirLight   { glm::vec4 Direction; glm::vec4 Color; };               // Color.a = intensity
	struct GpuPointLight { glm::vec4 Position;  glm::vec4 Color; };               // Position.w = range, Color.a = intensity
	struct GpuSpotLight  { glm::vec4 Position;  glm::vec4 Direction; glm::vec4 Color; glm::vec4 Params; };
	// Spot: Position.xyz = position, Direction.xyz = direction, Color.rgb = color,
	//       Params = (range, innerCos, outerCos, intensity)

	struct LightsData
	{
		GpuDirLight   DirLights[MAX_DIR_LIGHTS];
		GpuPointLight PointLights[MAX_POINT_LIGHTS];
		GpuSpotLight  SpotLights[MAX_SPOT_LIGHTS];
		uint32_t DirCount   = 0;
		uint32_t PointCount = 0;
		uint32_t SpotCount  = 0;
		uint32_t _Padding   = 0;
	};

	struct MeshSubmission
	{
		glm::mat4       Transform;
		Ref<MeshSource> Mesh;
		uint32_t        SubmeshIndex;
		Ref<Material>   Material;
		int             EntityID;
	};

	static constexpr uint32_t MAX_CASCADES = 4;

	// std140 mirror of the Shadows UBO block in Renderer3D_Lit.glsl.
	struct ShadowData
	{
		glm::mat4 LightViewProj[MAX_CASCADES];
		glm::vec4 CascadeSplits;            // far view-depth of each cascade
		glm::vec4 CascadeTexelWorld;        // world units per shadow texel, per cascade
		glm::vec4 CascadeDepthRange;        // world depth mapped to [0,1] per cascade
		uint32_t  CascadeCount = 0;
		uint32_t  VisualizeCascades = 0;
		float     LightSize = 0.08f;       // PCSS light size (from the sun's ShadowSoftness)
		float     DepthBias = 1.0f;        // multiplier on the shader depth bias
		float     CascadeBlend = 0.1f;     // boundary blend band (fraction of cascade extent)
		int32_t   BlockerSamples = 16;     // PCSS blocker search taps
		int32_t   PCFSamples = 16;         // PCSS / fixed-PCF filter taps
		int32_t   SoftShadows = 1;         // 1 = PCSS, 0 = fixed-kernel PCF
	};

	// std140 mirror of the SpotShadows UBO block. Indexed by spot light index.
	struct SpotShadowData
	{
		glm::mat4 LightViewProj[MAX_SPOT_LIGHTS];
		glm::vec4 Params[MAX_SPOT_LIGHTS]; // x = casts(0/1), y = lightSize, z = depthBias, w = 0
	};

	struct Renderer3DData
	{
		Ref<MeshSource>    CubeMesh;
		Ref<MeshSource>    SphereMesh;
		Ref<MeshSource>    PlaneMesh;

		Ref<Shader>        LitShader;
		Ref<UniformBuffer> CameraUniformBuffer;
		CameraData         CameraBuffer;

		Ref<UniformBuffer> LightsUniformBuffer;
		LightsData         LightsBuffer;
		bool               LightsDirty = true;

		Ref<Texture2D> WhiteTexture;
		Ref<Material>  DefaultMaterial;

		std::vector<MeshSubmission> Submissions;

		Ref<ShadowMap>     SunShadowMap;
		Ref<Shader>        ShadowDepthShader;
		Ref<UniformBuffer> ShadowUniformBuffer;
		ShadowData         ShadowBuffer;
		bool               SunCastsShadow = false;
		glm::vec3          SunDirection{ 0.0f, -1.0f, 0.0f };
		float              SunShadowSoftness = 0.5f;
		float              SunDepthBias = 1.0f;
		ShadowSettings     Shadows;

		Ref<ShadowMap>     SpotShadowMap;
		Ref<UniformBuffer> SpotShadowUniformBuffer;
		SpotShadowData     SpotShadowBuffer;
		bool               AnySpotCasts = false;

		static constexpr uint32_t SHADOW_SAMPLER_SLOT = 2;     // units 0=diffuse, 1=specular
		static constexpr uint32_t SPOT_SHADOW_SAMPLER_SLOT = 3;
		static constexpr float    SHADOW_DEPTH_EXTENT = 6.0f;  // ortho slab = radius * this each way along the light

		// Per-frame render-state cache (reset in BeginScene) to skip redundant GL state changes
		// across the many SubmitMesh calls of a frame.
		const Material* CurrentMaterial = nullptr;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init()
	{
		s_Data.CubeMesh   = MeshSource::CreateCube();
		s_Data.SphereMesh = MeshSource::CreateSphere();
		s_Data.PlaneMesh  = MeshSource::CreatePlane();

		s_Data.LitShader = Shader::Create("assets/shaders/Renderer3D_Lit.glsl");
		s_Data.ShadowDepthShader = Shader::Create("assets/shaders/Renderer3D_ShadowDepth.glsl");
		s_Data.SunShadowMap = ShadowMap::Create(s_Data.Shadows.ShadowMapResolution, s_Data.Shadows.CascadeCount);
		s_Data.SpotShadowMap = ShadowMap::Create(s_Data.Shadows.SpotShadowResolution, MAX_SPOT_LIGHTS);

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
		s_Data.LightsUniformBuffer = UniformBuffer::Create(sizeof(LightsData), 1);
		s_Data.ShadowUniformBuffer = UniformBuffer::Create(sizeof(ShadowData), 2);
		s_Data.SpotShadowUniformBuffer = UniformBuffer::Create(sizeof(SpotShadowData), 3);

		s_Data.WhiteTexture = Texture2D::Create(TextureSpecification());
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(Buffer(&whiteTextureData, sizeof(uint32_t)));

		s_Data.DefaultMaterial = CreateRef<Material>();

		// Map sampler uniforms to texture units 0 (diffuse) and 1 (specular) once.
		s_Data.LitShader->Bind();
		s_Data.LitShader->SetInt("u_DiffuseMap", 0);
		s_Data.LitShader->SetInt("u_SpecularMap", 1);
		s_Data.LitShader->SetInt("u_ShadowMap", (int)Renderer3DData::SHADOW_SAMPLER_SLOT);
		s_Data.LitShader->SetInt("u_SpotShadowMap", (int)Renderer3DData::SPOT_SHADOW_SAMPLER_SLOT);
	}

	void Renderer3D::Shutdown()
	{
	}

	void Renderer3D::BeginScene(const EditorCamera& camera)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.View = camera.GetViewMatrix();
		s_Data.CameraBuffer.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(CameraData));

		s_Data.LightsBuffer.DirCount = 0;
		s_Data.LightsBuffer.PointCount = 0;
		s_Data.LightsBuffer.SpotCount = 0;
		s_Data.LightsDirty = true;

		s_Data.Submissions.clear();
		s_Data.SunCastsShadow = false;
		s_Data.AnySpotCasts = false;
		s_Data.CurrentMaterial = nullptr;
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraBuffer.View = glm::inverse(transform);
		s_Data.CameraBuffer.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(CameraData));

		s_Data.LightsBuffer.DirCount = 0;
		s_Data.LightsBuffer.PointCount = 0;
		s_Data.LightsBuffer.SpotCount = 0;
		s_Data.LightsDirty = true;

		s_Data.Submissions.clear();
		s_Data.SunCastsShadow = false;
		s_Data.AnySpotCasts = false;
		s_Data.CurrentMaterial = nullptr;
	}

	void Renderer3D::SetShadowSettings(const ShadowSettings& settings)
	{
		s_Data.Shadows = settings;
		s_Data.Shadows.CascadeCount = glm::clamp(s_Data.Shadows.CascadeCount, 1u, ShadowSettings::MaxCascades);
		s_Data.Shadows.ShadowMapResolution = glm::max(s_Data.Shadows.ShadowMapResolution, 1u);

		if (s_Data.SunShadowMap->GetResolution() != s_Data.Shadows.ShadowMapResolution ||
			s_Data.SunShadowMap->GetLayerCount() != s_Data.Shadows.CascadeCount)
		{
			s_Data.SunShadowMap = ShadowMap::Create(s_Data.Shadows.ShadowMapResolution, s_Data.Shadows.CascadeCount);
		}

		if (s_Data.SpotShadowMap->GetResolution() != s_Data.Shadows.SpotShadowResolution)
			s_Data.SpotShadowMap = ShadowMap::Create(s_Data.Shadows.SpotShadowResolution, MAX_SPOT_LIGHTS);
	}

	// Bounding-sphere fit: rotation-invariant size + origin snapped to the texel grid -> no shimmer.
	static glm::mat4 FitOrthoToCorners(const glm::vec3 corners[8], const glm::vec3& lightDir,
		uint32_t resolution, float& outRadius)
	{
		glm::vec3 center(0.0f);
		for (int i = 0; i < 8; ++i) center += corners[i];
		center /= 8.0f;

		float radius = 0.0f;
		for (int i = 0; i < 8; ++i) radius = glm::max(radius, glm::length(corners[i] - center));
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

	static void ComputeCascades(const glm::mat4& cameraViewProjection, const glm::mat4& cameraView,
		const glm::vec3& lightDir, const ShadowSettings& settings, ShadowData& out)
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
		float camFar  = -(cameraView * glm::vec4(corners[1], 1.0f)).z;
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
			float tF = (dFar  - camNear) / range;

			glm::vec3 slice[8];
			for (int k = 0; k < 4; ++k)
			{
				const glm::vec3& nearC = corners[2 * k];
				const glm::vec3& farC  = corners[2 * k + 1];
				glm::vec3 edge = farC - nearC;
				slice[k]     = nearC + edge * tN;
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

	static RendererAPI::FaceCull ToFaceCull(ShadowCullMode mode)
	{
		switch (mode)
		{
			case ShadowCullMode::Front: return RendererAPI::FaceCull::Front;
			case ShadowCullMode::None:  return RendererAPI::FaceCull::None;
			case ShadowCullMode::Back:
			default:                    return RendererAPI::FaceCull::Back;
		}
	}

	static glm::mat4 ComputeSpotMatrix(const glm::vec3& position, const glm::vec3& direction,
		float range, float outerCutoffDegrees)
	{
		glm::vec3 dir = glm::normalize(direction);
		glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 proj = glm::perspective(glm::radians(2.0f * outerCutoffDegrees), 1.0f, 0.05f, glm::max(range, 0.1f));
		glm::mat4 view = glm::lookAt(position, position + dir, up);
		return proj * view;
	}

	void Renderer3D::EndScene()
	{
		// Upload any pending light data once for the frame.
		if (s_Data.LightsDirty)
		{
			s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(LightsData));
			s_Data.LightsDirty = false;
		}

		// Shared PCSS quality, read by both the sun cascades and spot shadows — must reach the
		// GPU even when no sun casts (spots still sample the Shadows UBO for these).
		s_Data.ShadowBuffer.CascadeBlend = s_Data.Shadows.CascadeBlend;
		s_Data.ShadowBuffer.BlockerSamples = (int32_t)s_Data.Shadows.BlockerSearchSamples;
		s_Data.ShadowBuffer.PCFSamples = (int32_t)s_Data.Shadows.PCFSamples;
		s_Data.ShadowBuffer.SoftShadows = s_Data.Shadows.SoftShadows ? 1 : 0;

		// Shadow depth pass: one draw set per cascade layer.
		if (s_Data.SunCastsShadow)
		{
			ComputeCascades(s_Data.CameraBuffer.ViewProjection, s_Data.CameraBuffer.View, s_Data.SunDirection,
				s_Data.Shadows, s_Data.ShadowBuffer);
			s_Data.ShadowBuffer.LightSize = s_Data.SunShadowSoftness * 0.16f;
			s_Data.ShadowBuffer.DepthBias = s_Data.SunDepthBias;
			s_Data.ShadowUniformBuffer->SetData(&s_Data.ShadowBuffer, sizeof(ShadowData));

			s_Data.ShadowDepthShader->Bind();
			RenderCommand::SetFaceCulling(ToFaceCull(s_Data.Shadows.CullMode));
			s_Data.SunShadowMap->BeginRenderPass();
			for (uint32_t c = 0; c < s_Data.Shadows.CascadeCount; ++c)
			{
				s_Data.ShadowDepthShader->SetMat4("u_LightSpaceMatrix", s_Data.ShadowBuffer.LightViewProj[c]);
				s_Data.SunShadowMap->BindLayer(c);
				for (const MeshSubmission& sub : s_Data.Submissions)
				{
					s_Data.ShadowDepthShader->SetMat4("u_Model", sub.Transform);
					const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
					RenderCommand::DrawIndexed(sub.Mesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
				}
			}
			s_Data.SunShadowMap->EndRenderPass();
			RenderCommand::SetFaceCulling(RendererAPI::FaceCull::Back);
		}
		else
		{
			// No caster: zero the cascade count so the lit shader skips shadows (avoids
			// sampling stale matrices / depth from a previous frame).
			s_Data.ShadowBuffer.CascadeCount = 0;
			s_Data.ShadowUniformBuffer->SetData(&s_Data.ShadowBuffer, sizeof(ShadowData));
		}

		// Always upload so a spot that stopped casting this frame clears its stale caster flag.
		s_Data.SpotShadowUniformBuffer->SetData(&s_Data.SpotShadowBuffer, sizeof(SpotShadowData));
		if (s_Data.AnySpotCasts)
		{
			s_Data.ShadowDepthShader->Bind();
			RenderCommand::SetFaceCulling(ToFaceCull(s_Data.Shadows.CullMode));
			s_Data.SpotShadowMap->BeginRenderPass();
			for (uint32_t i = 0; i < s_Data.LightsBuffer.SpotCount; ++i)
			{
				if (s_Data.SpotShadowBuffer.Params[i].x < 0.5f)
					continue;
				s_Data.ShadowDepthShader->SetMat4("u_LightSpaceMatrix", s_Data.SpotShadowBuffer.LightViewProj[i]);
				s_Data.SpotShadowMap->BindLayer(i);
				for (const MeshSubmission& sub : s_Data.Submissions)
				{
					s_Data.ShadowDepthShader->SetMat4("u_Model", sub.Transform);
					const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
					RenderCommand::DrawIndexed(sub.Mesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
				}
			}
			s_Data.SpotShadowMap->EndRenderPass();
			RenderCommand::SetFaceCulling(RendererAPI::FaceCull::Back);
		}

		s_Data.LitShader->Bind();
		s_Data.SunShadowMap->BindForSampling(Renderer3DData::SHADOW_SAMPLER_SLOT);
		s_Data.SpotShadowMap->BindForSampling(Renderer3DData::SPOT_SHADOW_SAMPLER_SLOT);

		s_Data.CurrentMaterial = nullptr;
		for (const MeshSubmission& sub : s_Data.Submissions)
		{
			s_Data.LitShader->SetMat4("u_Model", sub.Transform);
			glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(sub.Transform)));
			s_Data.LitShader->SetMat3("u_NormalMatrix", normalMatrix);
			s_Data.LitShader->SetInt("u_EntityID", sub.EntityID);

			const Material* mat = sub.Material.get();
			if (mat != s_Data.CurrentMaterial)
			{
				s_Data.LitShader->SetFloat3("u_DiffuseColor", mat->DiffuseColor);
				s_Data.LitShader->SetFloat3("u_SpecularColor", mat->SpecularColor);
				s_Data.LitShader->SetFloat("u_Shininess", mat->Shininess);

				Ref<Texture2D> diffuse = s_Data.WhiteTexture;
				if (mat->DiffuseMap != 0 && AssetManager::IsAssetHandleValid(mat->DiffuseMap))
					diffuse = AssetManager::GetAsset<Texture2D>(mat->DiffuseMap);

				Ref<Texture2D> specular = s_Data.WhiteTexture;
				if (mat->SpecularMap != 0 && AssetManager::IsAssetHandleValid(mat->SpecularMap))
					specular = AssetManager::GetAsset<Texture2D>(mat->SpecularMap);

				diffuse->Bind(0);
				specular->Bind(1);
				s_Data.CurrentMaterial = mat;
			}

			const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
			RenderCommand::DrawIndexed(sub.Mesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
		}
	}

	void Renderer3D::SubmitMesh(const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex,
		const Ref<Material>& material, int entityID)
	{
		if (!mesh || submeshIndex >= mesh->GetSubmeshes().size())
			return;

		const Ref<Material>& mat = material ? material : s_Data.DefaultMaterial;
		s_Data.Submissions.push_back({ transform, mesh, submeshIndex, mat, entityID });
	}

	Ref<Material> Renderer3D::GetDefaultMaterial()
	{
		return s_Data.DefaultMaterial;
	}

	void Renderer3D::RegisterBuiltInMeshes(EditorAssetManager& assetManager)
	{
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Cube,   s_Data.CubeMesh,   "Cube",   AssetType::Mesh);
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Sphere, s_Data.SphereMesh, "Sphere", AssetType::Mesh);
		assetManager.AddMemoryOnlyAsset(BuiltInMesh::Plane,  s_Data.PlaneMesh,  "Plane",  AssetType::Mesh);
	}

	void Renderer3D::SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity,
		bool castsShadows, float shadowSoftness, float depthBias)
	{
		if (s_Data.LightsBuffer.DirCount >= (int)MAX_DIR_LIGHTS)
			return;

		GpuDirLight& light = s_Data.LightsBuffer.DirLights[s_Data.LightsBuffer.DirCount++];
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color     = glm::vec4(color, intensity);
		s_Data.LightsDirty = true;

		// First directional light flagged as a caster becomes the shadow sun.
		if (castsShadows && !s_Data.SunCastsShadow)
		{
			s_Data.SunCastsShadow = true;
			s_Data.SunDirection = glm::normalize(direction);
			s_Data.SunShadowSoftness = shadowSoftness;
			s_Data.SunDepthBias = depthBias;
		}
	}

	void Renderer3D::SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range)
	{
		if (s_Data.LightsBuffer.PointCount >= (int)MAX_POINT_LIGHTS)
			return;

		GpuPointLight& light = s_Data.LightsBuffer.PointLights[s_Data.LightsBuffer.PointCount++];
		light.Position = glm::vec4(position, range);
		light.Color    = glm::vec4(color, intensity);
		s_Data.LightsDirty = true;
	}

	void Renderer3D::SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
		float intensity, float range, float innerCutoffDegrees, float outerCutoffDegrees,
		bool castsShadows, float shadowSoftness, float depthBias)
	{
		if (s_Data.LightsBuffer.SpotCount >= (int)MAX_SPOT_LIGHTS)
			return;

		float innerCos = glm::cos(glm::radians(innerCutoffDegrees));
		float outerCos = glm::cos(glm::radians(outerCutoffDegrees));

		uint32_t index = s_Data.LightsBuffer.SpotCount;
		GpuSpotLight& light = s_Data.LightsBuffer.SpotLights[s_Data.LightsBuffer.SpotCount++];
		light.Position  = glm::vec4(position, 0.0f);
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color     = glm::vec4(color, 0.0f);
		light.Params    = glm::vec4(range, innerCos, outerCos, intensity);
		s_Data.LightsDirty = true;

		if (castsShadows)
		{
			s_Data.SpotShadowBuffer.LightViewProj[index] = ComputeSpotMatrix(position, direction, range, outerCutoffDegrees);
			s_Data.SpotShadowBuffer.Params[index] = glm::vec4(1.0f, shadowSoftness * 0.16f, depthBias, 0.0f);
			s_Data.AnySpotCasts = true;
		}
		else
		{
			s_Data.SpotShadowBuffer.Params[index] = glm::vec4(0.0f);
		}
	}
}
