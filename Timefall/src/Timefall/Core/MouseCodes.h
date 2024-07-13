#pragma once

namespace Timefall
{
	typedef enum class MouseCode : uint16_t
	{
		// From glfw3.h
		Button0					= 0,
		Button1					= 1,
		Button2					= 2,
		Button3					= 3,
		Button4					= 4,
		Button5					= 5,
		Button6					= 6,
		Button7					= 7,

		ButtonLast				= Button7,
		ButtonLeft				= Button0,
		ButtonRight				= Button1,
		ButtonMiddle			= Button2
	} Mouse;

	inline std::ostream& operator<<(std::ostream& os, MouseCode mouseCode)
	{
		os << static_cast<int32_t>(mouseCode);
		return os;
	}
}

#define TF_MOUSE_BUTTON_0      ::Timefall::Mouse::Button0
#define TF_MOUSE_BUTTON_1      ::Timefall::Mouse::Button1
#define TF_MOUSE_BUTTON_2      ::Timefall::Mouse::Button2
#define TF_MOUSE_BUTTON_3      ::Timefall::Mouse::Button3
#define TF_MOUSE_BUTTON_4      ::Timefall::Mouse::Button4
#define TF_MOUSE_BUTTON_5      ::Timefall::Mouse::Button5
#define TF_MOUSE_BUTTON_6      ::Timefall::Mouse::Button6
#define TF_MOUSE_BUTTON_7      ::Timefall::Mouse::Button7
#define TF_MOUSE_BUTTON_LAST   ::Timefall::Mouse::ButtonLast
#define TF_MOUSE_BUTTON_LEFT   ::Timefall::Mouse::ButtonLeft
#define TF_MOUSE_BUTTON_RIGHT  ::Timefall::Mouse::ButtonRight
#define TF_MOUSE_BUTTON_MIDDLE ::Timefall::Mouse::ButtonMiddle