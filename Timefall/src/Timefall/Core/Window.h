#pragma once

#include "Timefall/Events/Event.h"

namespace Timefall
{
	struct WindowProps
	{
		std::string Title;
		uint32_t Width, Height;

		WindowProps(const std::string& title = "Timefall Engine", uint32_t width = 1600, uint32_t height = 900)
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

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		// Window attributes
		virtual void SetEventCallBack(const EventCallBackFn& callback) = 0;
		virtual void SetVsync(bool enabled) = 0;
		virtual bool IsVsync() const = 0;

		virtual void* GetNativeWindow() const = 0;

		static Scope<Window> Create(const WindowProps& props = WindowProps());
	};
}
