#pragma once

#include "Timefall/Core/Window.h"
#include "Timefall/Core/LayerStack.h"
#include "Timefall/Events/ApplicationEvent.h"

#include "Timefall/ImGui/ImGuiLayer.h"

int main(int argc, char** argv);

namespace Timefall
{
	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;

		const char* operator[](int index) const
		{
			TF_CORE_ASSERT(index < Count, "Index out of bounds");
			return Args[index];
		}
	};

	struct ApplicationSpecification
	{
		std::string Name = "Timefall Application";
		std::string WorkingDirectory;
		ApplicationCommandLineArgs CommandLineArgs;
	};

	class TF_API Application
	{
	public:
		Application(const ApplicationSpecification& specification);
		virtual ~Application();

		void OnEvent(Event& e);

		void PushLayer(Layer* layer);
		void PushOverlay(Layer* overlay);
		void Shutdown();

		ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

		static Application& Get() { return *s_Instance; }
		const ApplicationSpecification& GetSpecification() const { return m_Specification; }

		void SubmitToMainThread(const std::function<void()>& function);

		Window& GetWindow() const { return *m_Window; }

	private:
		void Run();
		bool OnWindowClose(WindowCloseEvent& e);
		bool OnWindowResize(WindowResizeEvent& e);

		void ExecuteMainThreadQueue();

	private:
		ApplicationSpecification m_Specification;
		Scope<Window> m_Window;
		ImGuiLayer* m_ImGuiLayer;
		bool m_Running = true;
		bool m_Minimized = false;
		LayerStack m_LayerStack;
		float m_LastFrameTime = 0.0f;

		std::vector<std::function<void()>> m_MainThreadQueue;
		std::mutex m_MainThreadQueueMutex;

	private:
		static Application* s_Instance;
		friend int ::main(int argc, char** argv);
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);
}
