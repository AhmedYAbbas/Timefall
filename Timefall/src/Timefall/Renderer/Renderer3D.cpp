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
		int DirCount   = 0;
		int PointCount = 0;
		int SpotCount  = 0;
		int _Padding   = 0;
	};

	struct MeshSubmission
	{
		glm::mat4       Transform;
		Ref<MeshSource> Mesh;
		uint32_t        SubmeshIndex;
		Ref<Material>   Material;
		int             EntityID;
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

		Ref<ShadowMap> SunShadowMap;
		Ref<Shader>    ShadowDepthShader;
		glm::mat4      SunLightSpaceMatrix = glm::mat4(1.0f);
		bool           SunCastsShadow = false;

		static constexpr uint32_t SHADOW_RESOLUTION = 2048;
		static constexpr uint32_t SHADOW_SAMPLER_SLOT = 2;  // units 0=diffuse,1=specular
		static constexpr float    SHADOW_MAX_DISTANCE = 50.0f;  // A.0: fit light frustum to this radius

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
		s_Data.SunShadowMap = ShadowMap::Create(Renderer3DData::SHADOW_RESOLUTION, 1);

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
		s_Data.LightsUniformBuffer = UniformBuffer::Create(sizeof(LightsData), 1);

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

	// Fit a single orthographic light-space matrix around the NEAR portion of the camera
	// frustum (out to maxShadowDistance). Phase A.0 only: no cascades, no texel snapping.
	// Bounding the distance keeps the ortho tight enough that (a) the occluder->receiver
	// depth gap stays well above the shader bias and (b) shadows occupy enough texels to be
	// visible. The full-frustum fit (camera far ~1000) was pathological on both counts.
	static glm::mat4 ComputeSunLightSpaceMatrix(const glm::mat4& cameraViewProjection, const glm::vec3& camPos,
		const glm::vec3& lightDir, float maxShadowDistance)
	{
		glm::mat4 invVP = glm::inverse(cameraViewProjection);

		// 8 world-space frustum corners from the NDC cube ([-1,1] in z for OpenGL).
		glm::vec3 corners[8];
		int i = 0;
		for (int x = 0; x < 2; ++x)
			for (int y = 0; y < 2; ++y)
				for (int z = 0; z < 2; ++z)
				{
					glm::vec4 pt = invVP * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
					corners[i++] = glm::vec3(pt) / pt.w;
				}

		// Pull any corner farther than maxShadowDistance from the camera in toward it, so the
		// fitted box covers only the near region (A.0 has no cascades to manage distance).
		for (glm::vec3& c : corners)
		{
			glm::vec3 v = c - camPos;
			float d = glm::length(v);
			if (d > maxShadowDistance)
				c = camPos + v * (maxShadowDistance / d);
		}

		glm::vec3 center(0.0f);
		for (const glm::vec3& c : corners) center += c;
		center /= 8.0f;

		glm::vec3 dir = glm::normalize(lightDir);
		glm::vec3 up = glm::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 lightView = glm::lookAt(center - dir, center, up);

		glm::vec3 minB(FLT_MAX), maxB(-FLT_MAX);
		for (const glm::vec3& c : corners)
		{
			glm::vec3 ls = glm::vec3(lightView * glm::vec4(c, 1.0f));
			minB = glm::min(minB, ls);
			maxB = glm::max(maxB, ls);
		}

		// Small additive pad along the light axis (toward the light = larger z here, since the
		// light looks down -z): extends the near plane so occluders just behind the slice still
		// cast, without blowing up the depth range like the old 10x multiply did.
		const float zPad = 0.25f * maxShadowDistance;
		minB.z -= zPad;
		maxB.z += zPad;

		glm::mat4 lightProj = glm::ortho(minB.x, maxB.x, minB.y, maxB.y, -maxB.z, -minB.z);
		return lightProj * lightView;
	}

	void Renderer3D::EndScene()
	{
		// Upload any pending light data once for the frame.
		if (s_Data.LightsDirty)
		{
			s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(LightsData));
			s_Data.LightsDirty = false;
		}

		// Phase A.0: the first directional light casts shadows (the CastsShadows toggle arrives in A.5).
		s_Data.SunCastsShadow = s_Data.LightsBuffer.DirCount > 0;

		// ---- Shadow depth pass ----
		if (s_Data.SunCastsShadow)
		{
			glm::vec3 sunDir = glm::vec3(s_Data.LightsBuffer.DirLights[0].Direction);
			s_Data.SunLightSpaceMatrix = ComputeSunLightSpaceMatrix(s_Data.CameraBuffer.ViewProjection,
				s_Data.CameraBuffer.CameraPosition, sunDir, Renderer3DData::SHADOW_MAX_DISTANCE);

			s_Data.ShadowDepthShader->Bind();
			s_Data.ShadowDepthShader->SetMat4("u_LightSpaceMatrix", s_Data.SunLightSpaceMatrix);

			s_Data.SunShadowMap->BeginRenderPass();
			s_Data.SunShadowMap->BindLayer(0);
			for (const MeshSubmission& sub : s_Data.Submissions)
			{
				s_Data.ShadowDepthShader->SetMat4("u_Model", sub.Transform);
				const Submesh& sm = sub.Mesh->GetSubmeshes()[sub.SubmeshIndex];
				RenderCommand::DrawIndexed(sub.Mesh->GetVertexArray(), sm.IndexCount, sm.BaseIndex, sm.BaseVertex);
			}
			s_Data.SunShadowMap->EndRenderPass();
		}

		// ---- Lit pass ----
		s_Data.LitShader->Bind();
		s_Data.LitShader->SetMat4("u_LightSpaceMatrix", s_Data.SunLightSpaceMatrix);
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
