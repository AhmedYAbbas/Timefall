#include "tfpch.h"

#include "Platform/Windows/WindowsInput.h"

#include "Timefall/Core/Application.h"

#include <GLFW/glfw3.h>

namespace Timefall
{
	bool WindowsInput::IsKeyPressedImpl(KeyCode key)
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const auto state = glfwGetKey(window, static_cast<int32_t>(key));
		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool WindowsInput::IsMouseButtonPressedImpl(MouseCode button)
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	std::pair<float, float> WindowsInput::GetMousePositionImpl()
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };
	}

	float WindowsInput::GetMouseXImpl()
	{
		auto[x, y] = GetMousePositionImpl();
		return x;
	}

	float WindowsInput::GetMouseYImpl()
	{
		auto[x, y] = GetMousePositionImpl();
		return y;
	}
}
