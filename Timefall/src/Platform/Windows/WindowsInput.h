#pragma once

#include "Timefall/Core/Input.h"

namespace Timefall
{
	class WindowsInput : public Input
	{
	protected:
		bool IsKeyPressedImpl(KeyCode key) override;

		bool IsMouseButtonPressedImpl(MouseCode button) override;
		std::pair<float, float> GetMousePositionImpl() override;
		float GetMouseXImpl() override;
		float GetMouseYImpl() override;
	};
}
