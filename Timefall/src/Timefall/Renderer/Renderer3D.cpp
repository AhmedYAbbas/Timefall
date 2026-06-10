#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/RenderCommand.h"

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

	struct Renderer3DData
	{
		Ref<Mesh>          CubeMesh;
		Ref<Mesh>          SphereMesh;
		Ref<Mesh>          PlaneMesh;

		Ref<Shader>        LitShader;
		Ref<UniformBuffer> CameraUniformBuffer;
		CameraData         CameraBuffer;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init()
	{
		s_Data.CubeMesh   = Mesh::CreateCube();
		s_Data.SphereMesh = Mesh::CreateSphere();
		s_Data.PlaneMesh  = Mesh::CreatePlane();

		s_Data.LitShader = Shader::Create("assets/shaders/Renderer3D_Lit.glsl");

		s_Data.CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
	}

	void Renderer3D::Shutdown()
	{
	}

	void Renderer3D::BeginScene(const EditorCamera& camera)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
		s_Data.CameraBuffer.CameraPosition = camera.GetPosition();
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(CameraData));
	}

	void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& transform)
	{
		s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
		s_Data.CameraBuffer.CameraPosition = glm::vec3(transform[3]);
		s_Data.CameraUniformBuffer->SetData(&s_Data.CameraBuffer, sizeof(CameraData));
	}

	void Renderer3D::EndScene()
	{
	}

	void Renderer3D::SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, int entityID)
	{
		if (!mesh)
			return;

		s_Data.LitShader->Bind();
		s_Data.LitShader->SetMat4("u_Model", transform);

		glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
		s_Data.LitShader->SetMat3("u_NormalMatrix", normalMatrix);
		s_Data.LitShader->SetInt("u_EntityID", entityID);

		const Ref<VertexArray>& vao = mesh->GetVertexArray();
		vao->Bind();
		RenderCommand::DrawIndexed(vao, mesh->GetIndexCount());
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
}
