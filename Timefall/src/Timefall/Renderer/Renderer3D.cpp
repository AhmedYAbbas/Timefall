#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/RenderCommand.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Asset/AssetManager.h"

#include <glm/glm.hpp>

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

	struct Renderer3DData
	{
		Ref<Mesh>          CubeMesh;
		Ref<Mesh>          SphereMesh;
		Ref<Mesh>          PlaneMesh;

		Ref<Shader>        LitShader;
		Ref<UniformBuffer> CameraUniformBuffer;
		CameraData         CameraBuffer;

		Ref<UniformBuffer> LightsUniformBuffer;
		LightsData         LightsBuffer;
		bool               LightsDirty = true;

		Ref<Texture2D> WhiteTexture;
		Ref<Material>  DefaultMaterial;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init()
	{
		s_Data.CubeMesh   = Mesh::CreateCube();
		s_Data.SphereMesh = Mesh::CreateSphere();
		s_Data.PlaneMesh  = Mesh::CreatePlane();

		s_Data.LitShader = Shader::Create("assets/shaders/Renderer3D_Lit.glsl");

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
	}

	void Renderer3D::EndScene()
	{
	}

	void Renderer3D::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, const Ref<Material>& material, int entityID)
	{
		if (s_Data.LightsDirty)
		{
			s_Data.LightsUniformBuffer->SetData(&s_Data.LightsBuffer, sizeof(LightsData));
			s_Data.LightsDirty = false;
		}

		if (!mesh)
			return;

		const Ref<Material>& mat = material ? material : s_Data.DefaultMaterial;

		s_Data.LitShader->Bind();
		s_Data.LitShader->SetMat4("u_Model", transform);

		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
		s_Data.LitShader->SetMat3("u_NormalMatrix", normalMatrix);
		s_Data.LitShader->SetInt("u_EntityID", entityID);

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

		const Ref<VertexArray>& vao = mesh->GetVertexArray();
		vao->Bind();
		RenderCommand::DrawIndexed(vao, mesh->GetIndexCount());
	}

	Ref<Material> Renderer3D::GetDefaultMaterial()
	{
		return s_Data.DefaultMaterial;
	}

	Ref<Mesh> Renderer3D::GetPrimitive(PrimitiveType type)
	{
		switch (type)
		{
			case PrimitiveType::Cube:   return s_Data.CubeMesh;
			case PrimitiveType::Sphere: return s_Data.SphereMesh;
			case PrimitiveType::Plane:  return s_Data.PlaneMesh;
		}
		return s_Data.CubeMesh;
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
