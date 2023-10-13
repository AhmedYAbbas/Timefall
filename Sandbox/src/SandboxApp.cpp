#include <Timefall.h>

#include "ImGui/imgui.h"

class ExampleLayer : public Timefall::Layer
{
public:
	ExampleLayer()
		: Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		m_VertexArray.reset(Timefall::VertexArray::Create());
		m_VertexArray->Bind();

		float vertices[] =
		{
			-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
			 0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
		};

		std::shared_ptr<Timefall::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(Timefall::VertexBuffer::Create());
		vertexBuffer->Bind();
		vertexBuffer->SetData(vertices, sizeof(vertices));


		Timefall::BufferLayout layout = {
			{ Timefall::ShaderDataType::Float3, "a_Position" },
			{ Timefall::ShaderDataType::Float4, "a_Color" }
		};
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);


		uint32_t indices[3] = {0, 1, 2};
		std::shared_ptr<Timefall::IndexBuffer> indexBuffer;
		indexBuffer.reset(Timefall::IndexBuffer::Create());
		indexBuffer->Bind();
		indexBuffer->SetData(indices, sizeof(indices) / sizeof(uint32_t));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		const std::string vertexSrc = R"(
			#version 330 core
		
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;
		
			uniform mat4 u_ViewProjection;

			out vec3 v_Position;
			out vec4 v_Color;
			
			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0f);
			}
		)";

		const std::string fragmentSrc = R"(
			#version 330 core
		
			out vec4 color;

			in vec3 v_Position;
			in vec4 v_Color;
			
			void main()
			{
				color = vec4(v_Position * 0.5f + 0.5f, 1.0f);
				color = v_Color;
			}
		)";

		m_Shader.reset(Timefall::Shader::Create(vertexSrc, fragmentSrc));
	}

	void OnUpdate() override
	{
		if (Timefall::Input::IsKeyPressed(TF_KEY_UP))
			m_CameraPosition.y += m_CameraMoveSpeed;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_DOWN))
			m_CameraPosition.y -= m_CameraMoveSpeed;

		if (Timefall::Input::IsKeyPressed(TF_KEY_LEFT))
			m_CameraPosition.x -= m_CameraMoveSpeed;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_RIGHT))
			m_CameraPosition.x += m_CameraMoveSpeed;

		if (Timefall::Input::IsKeyPressed(TF_KEY_A))
			m_CameraRotation += m_CameraRotationSpeed;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_D))
			m_CameraRotation -= m_CameraRotationSpeed;


		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation(m_CameraRotation);

		Timefall::Renderer::BeginScene(m_Camera);
		Timefall::Renderer::Submit(m_Shader, m_VertexArray);
		Timefall::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
	}

	void OnEvent(Timefall::Event& event) override
	{
	}

private:
	std::shared_ptr<Timefall::Shader> m_Shader;
	std::shared_ptr<Timefall::VertexArray> m_VertexArray;

	Timefall::OrthographicCamera m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraMoveSpeed = 0.02f;
	float m_CameraRotation = 0.0f;
	float m_CameraRotationSpeed = 1.0f;
};

class Sandbox : public Timefall::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
	}

	~Sandbox()
	{

	}
};

Timefall::Application* Timefall::CreateApplication()
{
	return new Sandbox();
}
