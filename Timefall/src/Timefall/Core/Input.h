#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Core/Keycodes.h"
#include "Timefall/Core/MouseCodes.h"

#include <glm/glm.hpp>

namespace Timefall
{
	class TF_API Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();

		// Per-frame edge detection (snapshot taken once per frame by Input::OnUpdate).
		static void OnUpdate();

		static bool IsKeyDown(KeyCode key);
		static bool IsKeyPressedThisFrame(KeyCode key);
		static bool IsKeyReleasedThisFrame(KeyCode key);

		static bool IsMouseButtonDown(MouseCode button);
		static bool IsMouseButtonPressedThisFrame(MouseCode button);
		static bool IsMouseButtonReleasedThisFrame(MouseCode button);

		// Viewport-relative mouse (top-left origin, pixels). Fed by the host (editor) each frame.
		static void SetViewportMousePosition(const glm::vec2& position);
		static glm::vec2 GetViewportMousePosition();
	};
}