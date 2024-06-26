#pragma once

#include "Timefall/Events/Event.h"

namespace Timefall
{
	struct WindowProps
	{
		std::string Title;
		unsigned int Width, Height;

		WindowProps(const std::string& title = "Timefall Engine", unsigned int width = 1280, unsigned int height = 720)
			: Title(title), Width(width), Height(height) 
		{
		}
	};

	// Interface representing a desktop system based window
	class Window
	{
	public:
		using EventCallBackFn = std::function<void(Event&)>;

		virtual ~Window() = default;

		virtual void OnUpdate() = 0;

		virtual unsigned int GetWidth() const = 0;
		virtual unsigned int GetHeight() const = 0;

		// Window attributes
		virtual void SetEventCallBack(const EventCallBackFn& callback) = 0;
		virtual void SetVsync(bool enabled) = 0;
		virtual bool IsVsync() const = 0;

		virtual void* GetNativeWindow() const = 0;

		static Scope<Window> Create(const WindowProps& props = WindowProps());
	};
}
