#include "tfpch.h"

#include "Timefall/Core/Input.h"

#include "Timefall/Core/Application.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <unordered_set>

namespace Timefall
{
	bool Input::IsKeyPressed(KeyCode key)
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const auto state = glfwGetKey(window, static_cast<int32_t>(key));
		return state == GLFW_PRESS;
	}

	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		const auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
		return state == GLFW_PRESS;
	}

	std::pair<float, float> Input::GetMousePosition()
	{
		const auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		return { (float)xpos, (float)ypos };
	}

	float Input::GetMouseX()
	{
		auto[x, y] = GetMousePosition();
		return x;
	}

	float Input::GetMouseY()
	{
		auto[x, y] = GetMousePosition();
		return y;
	}

	static constexpr int s_KeyCount = GLFW_KEY_LAST + 1;
	static constexpr int s_MouseButtonCount = GLFW_MOUSE_BUTTON_LAST + 1;

	static bool s_CurrentKeys[s_KeyCount] = { false };
	static bool s_PreviousKeys[s_KeyCount] = { false };
	static bool s_CurrentMouseButtons[s_MouseButtonCount] = { false };
	static bool s_PreviousMouseButtons[s_MouseButtonCount] = { false };
	static glm::vec2 s_ViewportMousePosition = { 0.0f, 0.0f };

	// Only keys that scripts actually query are polled. glfwGetKey raises GLFW_INVALID_ENUM for any
	// code that isn't a defined GLFW key (0..31, and gaps inside 256..348), so polling the full range
	// spams errors. Every KeyCode comes from glfw3.h, so each tracked key is a valid glfwGetKey input.
	static std::unordered_set<int> s_TrackedKeys;

	static bool KeyIndexValid(KeyCode key) { return (int)key >= 0 && (int)key < s_KeyCount; }
	static bool MouseIndexValid(MouseCode button) { return (int)button >= 0 && (int)button < s_MouseButtonCount; }

	static void TrackKey(KeyCode key) { if (KeyIndexValid(key)) s_TrackedKeys.insert((int)key); }

	void Input::OnUpdate()
	{
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());

		for (int key : s_TrackedKeys)
		{
			s_PreviousKeys[key] = s_CurrentKeys[key];
			s_CurrentKeys[key] = glfwGetKey(window, key) == GLFW_PRESS;
		}
		for (int button = 0; button < s_MouseButtonCount; ++button) // mouse buttons 0..LAST are all valid
		{
			s_PreviousMouseButtons[button] = s_CurrentMouseButtons[button];
			s_CurrentMouseButtons[button] = glfwGetMouseButton(window, button) == GLFW_PRESS;
		}
	}

	bool Input::IsKeyDown(KeyCode key) { TrackKey(key); return KeyIndexValid(key) && s_CurrentKeys[(int)key]; }
	bool Input::IsKeyPressedThisFrame(KeyCode key) { TrackKey(key); return KeyIndexValid(key) && s_CurrentKeys[(int)key] && !s_PreviousKeys[(int)key]; }
	bool Input::IsKeyReleasedThisFrame(KeyCode key) { TrackKey(key); return KeyIndexValid(key) && !s_CurrentKeys[(int)key] && s_PreviousKeys[(int)key]; }

	bool Input::IsMouseButtonDown(MouseCode button) { return MouseIndexValid(button) && s_CurrentMouseButtons[(int)button]; }
	bool Input::IsMouseButtonPressedThisFrame(MouseCode button) { return MouseIndexValid(button) && s_CurrentMouseButtons[(int)button] && !s_PreviousMouseButtons[(int)button]; }
	bool Input::IsMouseButtonReleasedThisFrame(MouseCode button) { return MouseIndexValid(button) && !s_CurrentMouseButtons[(int)button] && s_PreviousMouseButtons[(int)button]; }

	void Input::SetViewportMousePosition(const glm::vec2& position) { s_ViewportMousePosition = position; }
	glm::vec2 Input::GetViewportMousePosition() { return s_ViewportMousePosition; }
}
