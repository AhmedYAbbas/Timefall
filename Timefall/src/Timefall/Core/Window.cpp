#include "tfpch.h"
#include "Timefall/Core/Window.h"

#ifdef TF_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsWindow.h"
#endif

namespace Timefall
{
	Scope<Window> Window::Create(const WindowProps& props)
	{
	#ifdef TF_PLATFORM_WINDOWS
		return CreateScope<WindowsWindow>(props);
	#else
		TF_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}