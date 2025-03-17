#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Core/Keycodes.h"
#include "Timefall/Core/MouseCodes.h"

namespace Timefall
{
	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);
		static bool IsMouseButtonPressed(MouseCode button);
		static std::pair<float, float> GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
	};
}