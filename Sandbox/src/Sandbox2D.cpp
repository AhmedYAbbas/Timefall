#include "tfpch.h"
#include "Sandbox2D.h"

#include "imgui/imgui.h"
#include "Platform/OpenGL/OpenGLShader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
}

void Sandbox2D::OnAttach()
{
	m_SquareVA = Timefall::VertexArray::Create();
	m_SquareVA->Bind();

	float vertices[] =
	{
		 0.5f, -0.5f, 0.0f,
		 0.5f,  0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f
	};

	Timefall::Ref<Timefall::VertexBuffer> vertexBuffer;
	vertexBuffer = Timefall::VertexBuffer::Create();
	vertexBuffer->Bind();
	vertexBuffer->SetData(vertices, sizeof(vertices));

	vertexBuffer->SetLayout({
		{ Timefall::ShaderDataType::Float3, "a_Position" },
	});
	m_SquareVA->AddVertexBuffer(vertexBuffer);


	uint32_t indices[] = {0, 1, 2, 2, 3, 0};
	Timefall::Ref<Timefall::IndexBuffer> indexBuffer;
	indexBuffer = Timefall::IndexBuffer::Create();
	indexBuffer->Bind();
	indexBuffer->SetData(indices, sizeof(indices) / sizeof(uint32_t));
	m_SquareVA->SetIndexBuffer(indexBuffer);

	m_FlatColorShader = Timefall::Shader::Create("assets/shaders/FlatColor.glsl");
}

void Sandbox2D::OnDetach()
{
}

void Sandbox2D::OnUpdate(Timefall::Timestep ts)
{
	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Timefall::RenderCommand::Clear({0.1f, 0.1f, 0.1f, 1.0f});
	Timefall::Renderer::BeginScene(m_CameraController.GetCamera());

	std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_FlatColorShader)->Bind();
	std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat4("u_Color", m_SqaureColor);

	Timefall::Renderer::Submit(m_FlatColorShader, m_SquareVA, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

	Timefall::Renderer::EndScene();
}

void Sandbox2D::OnEvent(Timefall::Event& e)
{
	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Settings");
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SqaureColor));
	ImGui::End();
}
