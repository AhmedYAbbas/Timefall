#pragma once
	
#include "Timefall/Window.h"
#include "LayerStack.h"
#include "Events/ApplicationEvent.h"

namespace Timefall
{
	class TIMEFALL_API Application
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
		bool m_Running;
		LayerStack m_LayerStack;

	private:
		static Application* s_Instance;
	};

	Application* CreateApplication();
}
