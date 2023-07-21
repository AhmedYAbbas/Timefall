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

	private:
		bool OnWindowClose(WindowCloseEvent& e);

	private:
		std::unique_ptr<Window> m_Window;
		bool m_Running;
		LayerStack m_LayerStack;
	};

	Application* CreateApplication();
}
