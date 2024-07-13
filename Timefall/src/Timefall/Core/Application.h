#pragma once
	
#include "Timefall/Core/Window.h"
#include "Timefall/Core/LayerStack.h"
#include "Timefall/Events/ApplicationEvent.h"

#include "Timefall/ImGui/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Timefall
{
	class Application
	{
	public:
		Application();
		virtual ~Application();


		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);

		static Application& Get() { return *s_Instance; }
		Window& GetWindow() const { return *m_Window; }

	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

	private:
		Scope<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication();
}
