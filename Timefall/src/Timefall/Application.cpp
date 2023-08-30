#include "tfpch.h"
#include "Application.h"

#include "Log.h"

#include <glad/glad.h>

#include "Input.h"

namespace Timefall
{
	Application* Application::s_Instance = nullptr;

	Application::Application()
	{
		TF_CORE_ASSERT(!s_Instance, "Application already exists!");
		s_Instance = this;

		m_Window = std::unique_ptr<Window>(Window::Create());
		m_Window->SetEventCallBack(TF_BIND_EVENT_FN(&Application::OnEvent));

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);

		glGenVertexArrays(1, &m_VertexArray);
		glBindVertexArray(m_VertexArray);


		float vertices[3 * 3] =
		{
			-0.5f, -0.5f, 0.0f,
			 0.5f, -0.5f, 0.0f,
			 0.0f,  0.5f, 0.0f
		};
		m_VertexBuffer.reset(VertexBuffer::Create());
		m_VertexBuffer->Bind();
		m_VertexBuffer->SetData(vertices, sizeof(vertices));

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

		uint32_t indices[3] = { 0, 1, 2 };
		m_IndexBuffer.reset(IndexBuffer::Create());
		m_IndexBuffer->Bind();
		m_IndexBuffer->SetData(indices, sizeof(indices) / sizeof(uint32_t));

		const std::string vertexSrc = R"(
			#version 330 core
		
			layout(location = 0) in vec4 a_Position;
		
			out vec4 v_Position;
			
			void main()
			{
				v_Position = a_Position;
				gl_Position = a_Position;
			}
		)";

		const std::string fragmentSrc = R"(
			#version 330 core
		
			out vec4 color;

			in vec4 v_Position;
			
			void main()
			{
				color = v_Position * 0.5 + 0.5;
			}
		)";

		m_Shader.reset(Shader::Create(vertexSrc, fragmentSrc));
	}

	Application::~Application()
	{

	}

	void Application::PushLayer(Layer* layer)
	{
		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* overlay)
	{
		m_LayerStack.PushOverlay(overlay);
	}

	void Application::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(TF_BIND_EVENT_FN(&Application::OnWindowClose));

		for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
		{
			(*--it)->OnEvent(e);
			if (e.Handled)
				break;
		}
	}


	void Application::Run()
	{
		while (m_Running)
		{
			glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_Shader->Bind();
			glBindVertexArray(m_VertexArray);
			glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

			for (Layer* layer : m_LayerStack)
				layer->OnUpdate();

			m_ImGuiLayer->Begin();
			for (Layer* layer : m_LayerStack)
				layer->OnImGuiRender();
			m_ImGuiLayer->End();

			m_Window->OnUpdate();
		}
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		m_Running = false;
		return true;
	}

}
