#include <Timefall.h>

#include "Platform/OpenGL/OpenGLShader.h"
#include "Platform/OpenGL/OpenGLTexture.h"

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class ExampleLayer : public Timefall::Layer
{
public:
	ExampleLayer()
		: Layer("Example"), m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		m_VertexArray = Timefall::VertexArray::Create();
		m_VertexArray->Bind();

		float vertices[] =
		{
			 0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f,	 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f,	 0.0f, 1.0f,
			-0.5f, -0.5f, 0.0f,  0.0f, 0.0f
		};

		Timefall::Ref<Timefall::VertexBuffer> vertexBuffer;
		vertexBuffer = Timefall::VertexBuffer::Create();
		vertexBuffer->Bind();
		vertexBuffer->SetData(vertices, sizeof(vertices));


		vertexBuffer->SetLayout({
			{ Timefall::ShaderDataType::Float3, "a_Position" },
			{ Timefall::ShaderDataType::Float2, "a_TexCoord" }
		});
		m_VertexArray->AddVertexBuffer(vertexBuffer);


		uint32_t indices[] = {0, 1, 2, 2, 3, 0};
		Timefall::Ref<Timefall::IndexBuffer> indexBuffer;
		indexBuffer = Timefall::IndexBuffer::Create();
		indexBuffer->Bind();
		indexBuffer->SetData(indices, sizeof(indices) / sizeof(uint32_t));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		const std::string flatColorVertexShaderSrc = R"(
			#version 330 core
		
			layout(location = 0) in vec3 a_Position;
		
			uniform mat4 u_ViewProjection;
			uniform mat4 u_Transform;

			void main()
			{
				gl_Position = u_ViewProjection * u_Transform * vec4(a_Position, 1.0f);
			}
		)";

		const std::string flatColorFragmentShaderSrc = R"(
			#version 330 core
		
			uniform vec3 u_Color;

			out vec4 color;
			
			void main()
			{
				color = vec4(u_Color, 1.0f);
			}
		)";

		m_FlatColorShader = Timefall::Shader::Create(flatColorVertexShaderSrc, flatColorFragmentShaderSrc);
		m_TextureShader = Timefall::Shader::Create("assets/shaders/Texture.glsl");

		m_Texture = Timefall::Texture2D::Create("assets/textures/Naiyra.jpg");
		m_AlphaTexture = Timefall::Texture2D::Create("assets/textures/Fish.png");

		std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_TextureShader)->Bind();
		std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_TextureShader)->UploadUniformInt("u_Texture", 0);
	}

	void OnUpdate(Timefall::Timestep ts) override
	{
		if (Timefall::Input::IsKeyPressed(TF_KEY_UP))
			m_CameraPosition.y += m_CameraMoveSpeed * ts;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_DOWN))
			m_CameraPosition.y -= m_CameraMoveSpeed * ts;

		if (Timefall::Input::IsKeyPressed(TF_KEY_LEFT))
			m_CameraPosition.x -= m_CameraMoveSpeed * ts;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_RIGHT))
			m_CameraPosition.x += m_CameraMoveSpeed * ts;

		if (Timefall::Input::IsKeyPressed(TF_KEY_A))
			m_CameraRotation += m_CameraRotationSpeed * ts;
		else if (Timefall::Input::IsKeyPressed(TF_KEY_D))
			m_CameraRotation -= m_CameraRotationSpeed * ts;

		Timefall::RenderCommand::Clear({0.1f, 0.1f, 0.1f, 1.0f});

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation(m_CameraRotation);

		Timefall::Renderer::BeginScene(m_Camera);

		static glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));

		std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_FlatColorShader)->Bind();
		std::dynamic_pointer_cast<Timefall::OpenGLShader>(m_FlatColorShader)->UploadUniformFloat3("u_Color", m_SqaureColor);

		for (int y = 0; y < 20; y++)
		{
			for (int x = 0; x < 20; x++)
			{
				glm::vec3 pos(x * 0.11f, y * 0.11f, 0.0f);
				glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * scale;
				Timefall::Renderer::Submit(m_FlatColorShader, m_VertexArray, transform);
			}
		}

		m_Texture->Bind();
		Timefall::Renderer::Submit(m_TextureShader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));
		m_AlphaTexture->Bind();
		Timefall::Renderer::Submit(m_TextureShader, m_VertexArray, glm::scale(glm::mat4(1.0f), glm::vec3(1.5f)));

		Timefall::Renderer::EndScene();
	}

	void OnImGuiRender() override
	{
		ImGui::Begin("Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SqaureColor));
		ImGui::End();
	}

	void OnEvent(Timefall::Event& event) override
	{
	}

private:
	Timefall::Ref<Timefall::Shader> m_FlatColorShader;
	Timefall::Ref<Timefall::Shader> m_TextureShader;
	Timefall::Ref<Timefall::VertexArray> m_VertexArray;

	Timefall::Ref<Timefall::Texture> m_Texture, m_AlphaTexture;

	glm::vec3 m_SqaureColor = {0.2f, 0.3f, 0.8f};

	Timefall::OrthographicCamera m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraMoveSpeed = 5.f;
	float m_CameraRotation = 0.0f;
	float m_CameraRotationSpeed = 180.0f;
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
