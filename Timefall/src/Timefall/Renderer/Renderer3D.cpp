#include "tfpch.h"

#include "Timefall/Renderer/Renderer3D.h"

#include "Timefall/Renderer/VertexArray.h"
#include "Timefall/Renderer/Buffer.h"
#include "Timefall/Renderer/Shader.h"
#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/RenderCommand.h"

#include <glm/glm.hpp>

namespace Timefall
{
	struct CubeVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TexCoord;
	};

	// std140 layout matching the Camera UBO block in Renderer3D_Lit.glsl.
	// mat4 (64B) + vec3 (12B) + 4B pad = 80B.
	struct CameraData
	{
		glm::mat4 ViewProjection;
		glm::vec3 CameraPosition;
		float _Padding = 0.0f;
	};

	struct Renderer3DData
	{
		Ref<VertexArray>   CubeVertexArray;
		Ref<Shader>        LitShader;
		Ref<UniformBuffer> CameraUniformBuffer;
		CameraData         CameraBuffer;
	};

	static Renderer3DData s_Data;

	void Renderer3D::Init()
	{
		// 24 vertices (4 per face) so each face has its own normal.
		CubeVertex vertices[24] = {
			// Front (+Z)
			{ {-0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {0,0} },
			{ { 0.5f,-0.5f, 0.5f}, { 0, 0, 1}, {1,0} },
			{ { 0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {1,1} },
			{ {-0.5f, 0.5f, 0.5f}, { 0, 0, 1}, {0,1} },
			// Back (-Z)
			{ { 0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {0,0} },
			{ {-0.5f,-0.5f,-0.5f}, { 0, 0,-1}, {1,0} },
			{ {-0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {1,1} },
			{ { 0.5f, 0.5f,-0.5f}, { 0, 0,-1}, {0,1} },
			// Left (-X)
			{ {-0.5f,-0.5f,-0.5f}, {-1, 0, 0}, {0,0} },
			{ {-0.5f,-0.5f, 0.5f}, {-1, 0, 0}, {1,0} },
			{ {-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, {-1, 0, 0}, {0,1} },
			// Right (+X)
			{ { 0.5f,-0.5f, 0.5f}, { 1, 0, 0}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, { 1, 0, 0}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, { 1, 0, 0}, {1,1} },
			{ { 0.5f, 0.5f, 0.5f}, { 1, 0, 0}, {0,1} },
			// Top (+Y)
			{ {-0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {0,0} },
			{ { 0.5f, 0.5f, 0.5f}, { 0, 1, 0}, {1,0} },
			{ { 0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {1,1} },
			{ {-0.5f, 0.5f,-0.5f}, { 0, 1, 0}, {0,1} },
			// Bottom (-Y)
			{ {-0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {0,0} },
			{ { 0.5f,-0.5f,-0.5f}, { 0,-1, 0}, {1,0} },
			{ { 0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {1,1} },
			{ {-0.5f,-0.5f, 0.5f}, { 0,-1, 0}, {0,1} },
		};

		uint32_t indices[36];
		for (uint32_t face = 0; face < 6; face++)
		{
			uint32_t v = face * 4;
			uint32_t i = face * 6;
			indices[i + 0] = v + 0; indices[i + 1] = v + 1; indices[i + 2] = v + 2;
			indices[i + 3] = v + 2; indices[i + 4] = v + 3; indices[i + 5] = v + 0;
		}

		s_Data.CubeVertexArray = VertexArray::Create();
		s_Data.CubeVertexArray->Bind();

		Ref<VertexBuffer> vbo = VertexBuffer::Create((float*)vertices, sizeof(vertices));
		vbo->SetLayout({
			{ ShaderDataType::Float3, "a_Position" },
			{ ShaderDataType::Float3, "a_Normal"   },
			{ ShaderDataType::Float2, "a_TexCoord" },
		});
		s_Data.CubeVertexArray->AddVertexBuffer(vbo);

		Ref<IndexBuffer> ibo = IndexBuffer::Create(indices, 36);
		s_Data.CubeVertexArray->SetIndexBuffer(ibo);

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

	void Renderer3D::EndScene()
	{
	}

	void Renderer3D::DrawTestCube(const glm::mat4& transform)
	{
		s_Data.LitShader->Bind();
		s_Data.LitShader->SetMat4("u_Model", transform);

		s_Data.CubeVertexArray->Bind();
		RenderCommand::DrawIndexed(s_Data.CubeVertexArray, 36);
	}
}
