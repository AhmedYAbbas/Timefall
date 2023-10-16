#pragma once
	
#include "Timefall/Window.h"
#include "LayerStack.h"
#include "Events/ApplicationEvent.h"

#include "ImGui/ImGuiLayer.h"

namespace Timefall
{
	class Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		static Application& Get() { return *s_Instance; }
		Window& GetWindow() const { return *m_Window; }
	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}
