#pragma once

#include "Timefall/Window.h"
#include "Timefall/Renderer/GraphicsContext.h"

#include <GLFW/glfw3.h>

namespace Timefall
{
	class WindowsWindow : public Window
	{
	public:
		WindowsWindow(const WindowProps& props);
		virtual ~WindowsWindow();

		void OnUpdate() override;

		unsigned int GetWidth() const override { return m_Data.Width; }
		unsigned int GetHeight() const override { return m_Data.Height; }

		// Window attributes
		void SetEventCallBack(const EventCallBackFn& callback) override { m_Data.EventCallBack = callback; }
		void SetVsync(bool enabled) override;
		bool IsVsync() const override { return m_Data.Vsync; }

		void* GetNativeWindow() const override  { return m_Window; }

	private:
		void Init(const WindowProps& props);
		void Shutdown() const;

	private:
		GLFWwindow* m_Window;
		GraphicsContext* m_Context;

		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
			bool Vsync;

			EventCallBackFn EventCallBack;
		};

		WindowData m_Data;
	};
}

