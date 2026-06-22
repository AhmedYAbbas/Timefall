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

#include <cfloat>

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
		uint32_t  CascadeCount = 0;
		uint32_t  VisualizeCascades = 0;
		glm::vec2 _Pad{ 0.0f };
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

		static constexpr uint32_t SHADOW_RESOLUTION   = 2048;
		static constexpr uint32_t SHADOW_SAMPLER_SLOT = 2;     // units 0=diffuse, 1=specular
		static constexpr uint32_t CASCADE_COUNT       = 4;     // <= MAX_CASCADES
		static constexpr float    SHADOW_MAX_DISTANCE = 100.0f;// cascades split within [near .. this]
		static constexpr float    SPLIT_LAMBDA        = 0.85f;
		static constexpr bool     VISUALIZE_CASCADES  = false; // debug-tint cascades

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
		s_Data.SunShadowMap = ShadowMap::Create(Renderer3DData::SHADOW_RESOLUTION, Renderer3DData::CASCADE_COUNT);

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
		s_Data.LightsUniformBuffer = UniformBuffer::Create(sizeof(LightsData), 1);
		s_Data.ShadowUniformBuffer = UniformBuffer::Create(sizeof(ShadowData), 2);

		s_Data.WhiteTexture = Texture2D::Create(TextureSpecification());
		uint32_t whiteTextureData = 0xffffffff;
		s_Data.WhiteTexture->SetData(Buffer(&whiteTextureData, sizeof(uint32_t)));

		s_Data.DefaultMaterial = CreateRef<Material>();

		// Map sampler uniforms to texture units 0 (diffuse) and 1 (specular) once.
		s_Data.LitShader->Bind();
		s_Data.LitShader->SetInt("u_DiffuseMap", 0);
		s_Data.LitShader->SetInt("u_SpecularMap", 1);
		s_Data.LitShader->SetInt("u_ShadowMap", (int)Renderer3DData::SHADOW_SAMPLER_SLOT);
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
		s_Data.CurrentMaterial = nullptr;
	}

	// Plain AABB fit around one cascade slice's corners (bounding-sphere stabilization is A.2).
	static glm::mat4 FitOrthoToCorners(const glm::vec3 corners[8], const glm::vec3& lightDir, float zPad)
	{
		glm::vec3 center(0.0f);
		for (int i = 0; i < 8; ++i) center += corners[i];
		center /= 8.0f;

		glm::vec3 dir = glm::normalize(lightDir);
		glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 lightView = glm::lookAt(center - dir, center, up);

		glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);
		for (int i = 0; i < 8; ++i)
		{
			glm::vec3 ls = glm::vec3(lightView * glm::vec4(corners[i], 1.0f));
			minB = glm::min(minB, ls);
			maxB = glm::max(maxB, ls);
		}

		// Pad the near plane toward the light so occluders just behind the slice still cast.
		minB.z -= zPad;
		maxB.z += zPad;

		glm::mat4 lightProj = glm::ortho(minB.x, maxB.x, minB.y, maxB.y, -maxB.z, -minB.z);
		return lightProj * lightView;
	}

	static void ComputeCascades(const glm::mat4& cameraViewProjection, const glm::mat4& cameraView,
		const glm::vec3& lightDir, float maxShadowDistance, uint32_t cascadeCount, ShadowData& out)
	{
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
			splits[c] = glm::mix(uniD, logD, Renderer3DData::SPLIT_LAMBDA);
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

			out.LightViewProj[c] = FitOrthoToCorners(slice, lightDir, 0.25f * (dFar - dNear) + 5.0f);
			out.CascadeSplits[c] = dFar;
			dNear = dFar;
		}

		out.CascadeCount = cascadeCount;
		out.VisualizeCascades = Renderer3DData::VISUALIZE_CASCADES ? 1u : 0u;
	}

	void Renderer3D::EndScene()
	{
		// Upload any pending light data once for the frame.
		if (s_Data.LightsDirty)
		{
			s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(LightsData));
			s_Data.LightsDirty = false;
		}

		s_Data.SunCastsShadow = s_Data.LightsBuffer.DirCount > 0;

		// Shadow depth pass: one draw set per cascade layer.
		if (s_Data.SunCastsShadow)
		{
			glm::vec3 sunDir = glm::vec3(s_Data.LightsBuffer.DirLights[0].Direction);
			ComputeCascades(s_Data.CameraBuffer.ViewProjection, s_Data.CameraBuffer.View, sunDir,
				Renderer3DData::SHADOW_MAX_DISTANCE, Renderer3DData::CASCADE_COUNT, s_Data.ShadowBuffer);
			s_Data.ShadowUniformBuffer->SetData(&s_Data.ShadowBuffer, sizeof(ShadowData));

			s_Data.ShadowDepthShader->Bind();
			s_Data.SunShadowMap->BeginRenderPass();
			for (uint32_t c = 0; c < Renderer3DData::CASCADE_COUNT; ++c)
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
		}

		s_Data.LitShader->Bind();
		s_Data.SunShadowMap->BindForSampling(Renderer3DData::SHADOW_SAMPLER_SLOT);

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

	void Renderer3D::SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity)
	{
		if (s_Data.LightsBuffer.DirCount >= (int)MAX_DIR_LIGHTS)
			return;

		GpuDirLight& light = s_Data.LightsBuffer.DirLights[s_Data.LightsBuffer.DirCount++];
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color     = glm::vec4(color, intensity);
		s_Data.LightsDirty = true;
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
		float intensity, float range, float innerCutoffDegrees, float outerCutoffDegrees)
	{
		if (s_Data.LightsBuffer.SpotCount >= (int)MAX_SPOT_LIGHTS)
			return;

		float innerCos = glm::cos(glm::radians(innerCutoffDegrees));
		float outerCos = glm::cos(glm::radians(outerCutoffDegrees));

		GpuSpotLight& light = s_Data.LightsBuffer.SpotLights[s_Data.LightsBuffer.SpotCount++];
		light.Position  = glm::vec4(position, 0.0f);
		light.Direction = glm::vec4(glm::normalize(direction), 0.0f);
		light.Color     = glm::vec4(color, 0.0f);
		light.Params    = glm::vec4(range, innerCos, outerCos, intensity);
		s_Data.LightsDirty = true;
	}
}
