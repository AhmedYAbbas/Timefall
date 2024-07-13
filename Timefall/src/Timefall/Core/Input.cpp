#include "tfpch.h"
#include "Timefall/Core/Input.h"

#ifdef TF_PLATFORM_WINDOWS
#include "Platform/Windows/WindowsInput.h"
#endif

namespace Timefall
{
	Scope<Input> Input::s_Instance = Input::Create();

	Scope<Input> Input::Create()
	{
	#ifdef TF_PLATFORM_WINDOWS
		return CreateScope<WindowsInput>();
	#else
		TF_CORE_ASSERT(false, "Unknown platform!");
		return nullptr;
	#endif
	}
}