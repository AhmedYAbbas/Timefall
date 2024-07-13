#pragma once

namespace Timefall
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space					= 32,
		Apostrophe				= 39, /* ' */
		Comma					= 44, /* , */
		Minus					= 45, /* - */
		Period					= 46, /* . */
		Slash					= 47, /* / */

		D0						= 48, /* 0 */
		D1						= 49, /* 1 */
		D2						= 50, /* 2 */
		D3						= 51, /* 3 */
		D4						= 52, /* 4 */
		D5						= 53, /* 5 */
		D6						= 54, /* 6 */
		D7						= 55, /* 7 */
		D8						= 56, /* 8 */
		D9						= 57, /* 9 */

		Semicolon				= 59, /* ; */
		Equal					= 61, /* = */

		A						= 65,
		B						= 66,
		C						= 67,
		D						= 68,
		E						= 69,
		F						= 70,
		G						= 71,
		H						= 72,
		I						= 73,
		J						= 74,
		K						= 75,
		L						= 76,
		M						= 77,
		N						= 78,
		O						= 79,
		P						= 80,
		Q						= 81,
		R						= 82,
		S						= 83,
		T						= 84,
		U						= 85,
		V						= 86,
		W						= 87,
		X						= 88,
		Y						= 89,
		Z						= 90,

		LeftBracket				= 91,  /* [ */
		Backslash				= 92,  /* \ */
		RightBracket			= 93,  /* ] */
		GraveAccent				= 96,  /* ` */

		World1					= 161, /* non-US #1 */
		World2					= 162, /* non-US #2 */

		/* Function keys */
		Escape					= 256,
		Enter					= 257,
		Tab						= 258,
		Backspace				= 259,
		Insert					= 260,
		Delete					= 261,
		Right					= 262,
		Left					= 263,
		Down					= 264,
		Up						= 265,
		PageUp					= 266,
		PageDown				= 267,
		Home					= 268,
		End						= 269,
		CapsLock				= 280,
		ScrollLock				= 281,
		NumLock					= 282,
		PrintScreen				= 283,
		Pause					= 284,
		F1						= 290,
		F2						= 291,
		F3						= 292,
		F4						= 293,
		F5						= 294,
		F6						= 295,
		F7						= 296,
		F8						= 297,
		F9						= 298,
		F10						= 299,
		F11						= 300,
		F12						= 301,
		F13						= 302,
		F14						= 303,
		F15						= 304,
		F16						= 305,
		F17						= 306,
		F18						= 307,
		F19						= 308,
		F20						= 309,
		F21						= 310,
		F22						= 311,
		F23						= 312,
		F24						= 313,
		F25						= 314,

		/* Keypad */
		KP0						= 320,
		KP1						= 321,
		KP2						= 322,
		KP3						= 323,
		KP4						= 324,
		KP5						= 325,
		KP6						= 326,
		KP7						= 327,
		KP8						= 328,
		KP9						= 329,
		KPDecimal				= 330,
		KPDivide				= 331,
		KPMultiply				= 332,
		KPSubtract				= 333,
		KPAdd					= 334,
		KPEnter					= 335,
		KPEqual					= 336,

		LeftShift				= 340,
		LeftControl				= 341,
		LeftAlt					= 342,
		LeftSuper				= 343,
		RightShift				= 344,
		RightControl			= 345,
		RightAlt				= 346,
		RightSuper				= 347,
		Menu					= 348
	} Key;

	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}
}

// From glfw3.h
#define TF_KEY_SPACE           ::Timefall::Key::Space
#define TF_KEY_APOSTROPHE      ::Timefall::Key::Apostrophe    /* ' */
#define TF_KEY_COMMA           ::Timefall::Key::Comma         /* , */
#define TF_KEY_MINUS           ::Timefall::Key::Minus         /* - */
#define TF_KEY_PERIOD          ::Timefall::Key::Period        /* . */
#define TF_KEY_SLASH           ::Timefall::Key::Slash         /* / */
#define TF_KEY_0               ::Timefall::Key::D0
#define TF_KEY_1               ::Timefall::Key::D1
#define TF_KEY_2               ::Timefall::Key::D2
#define TF_KEY_3               ::Timefall::Key::D3
#define TF_KEY_4               ::Timefall::Key::D4
#define TF_KEY_5               ::Timefall::Key::D5
#define TF_KEY_6               ::Timefall::Key::D6
#define TF_KEY_7               ::Timefall::Key::D7
#define TF_KEY_8               ::Timefall::Key::D8
#define TF_KEY_9               ::Timefall::Key::D9
#define TF_KEY_SEMICOLON       ::Timefall::Key::Semicolon     /* ; */
#define TF_KEY_EQUAL           ::Timefall::Key::Equal         /* = */
#define TF_KEY_A               ::Timefall::Key::A
#define TF_KEY_B               ::Timefall::Key::B
#define TF_KEY_C               ::Timefall::Key::C
#define TF_KEY_D               ::Timefall::Key::D
#define TF_KEY_E               ::Timefall::Key::E
#define TF_KEY_F               ::Timefall::Key::F
#define TF_KEY_G               ::Timefall::Key::G
#define TF_KEY_H               ::Timefall::Key::H
#define TF_KEY_I               ::Timefall::Key::I
#define TF_KEY_J               ::Timefall::Key::J
#define TF_KEY_K               ::Timefall::Key::K
#define TF_KEY_L               ::Timefall::Key::L
#define TF_KEY_M               ::Timefall::Key::M
#define TF_KEY_N               ::Timefall::Key::N
#define TF_KEY_O               ::Timefall::Key::O
#define TF_KEY_P               ::Timefall::Key::P
#define TF_KEY_Q               ::Timefall::Key::Q
#define TF_KEY_R               ::Timefall::Key::R
#define TF_KEY_S               ::Timefall::Key::S
#define TF_KEY_T               ::Timefall::Key::T
#define TF_KEY_U               ::Timefall::Key::U
#define TF_KEY_V               ::Timefall::Key::V
#define TF_KEY_W               ::Timefall::Key::W
#define TF_KEY_X               ::Timefall::Key::X
#define TF_KEY_Y               ::Timefall::Key::Y
#define TF_KEY_Z               ::Timefall::Key::Z
#define TF_KEY_LEFT_BRACKET    ::Timefall::Key::LeftBracket   /* [ */
#define TF_KEY_BACKSLASH       ::Timefall::Key::Backslash     /* \ */
#define TF_KEY_RIGHT_BRACKET   ::Timefall::Key::RightBracket  /* ] */
#define TF_KEY_GRAVE_ACCENT    ::Timefall::Key::GraveAccent   /* ` */
#define TF_KEY_WORLD_1         ::Timefall::Key::World1        /* non-US #1 */
#define TF_KEY_WORLD_2         ::Timefall::Key::World2        /* non-US #2 */

/* Function keys */
#define TF_KEY_ESCAPE          ::Timefall::Key::Escape
#define TF_KEY_ENTER           ::Timefall::Key::Enter
#define TF_KEY_TAB             ::Timefall::Key::Tab
#define TF_KEY_BACKSPACE       ::Timefall::Key::Backspace
#define TF_KEY_INSERT          ::Timefall::Key::Insert
#define TF_KEY_DELETE          ::Timefall::Key::Delete
#define TF_KEY_RIGHT           ::Timefall::Key::Right
#define TF_KEY_LEFT            ::Timefall::Key::Left
#define TF_KEY_DOWN            ::Timefall::Key::Down
#define TF_KEY_UP              ::Timefall::Key::Up
#define TF_KEY_PAGE_UP         ::Timefall::Key::PageUp
#define TF_KEY_PAGE_DOWN       ::Timefall::Key::PageDown
#define TF_KEY_HOME            ::Timefall::Key::Home
#define TF_KEY_END             ::Timefall::Key::End
#define TF_KEY_CAPS_LOCK       ::Timefall::Key::CapsLock
#define TF_KEY_SCROLL_LOCK     ::Timefall::Key::ScrollLock
#define TF_KEY_NUM_LOCK        ::Timefall::Key::NumLock
#define TF_KEY_PRINT_SCREEN    ::Timefall::Key::PrintScreen
#define TF_KEY_PAUSE           ::Timefall::Key::Pause
#define TF_KEY_F1              ::Timefall::Key::F1
#define TF_KEY_F2              ::Timefall::Key::F2
#define TF_KEY_F3              ::Timefall::Key::F3
#define TF_KEY_F4              ::Timefall::Key::F4
#define TF_KEY_F5              ::Timefall::Key::F5
#define TF_KEY_F6              ::Timefall::Key::F6
#define TF_KEY_F7              ::Timefall::Key::F7
#define TF_KEY_F8              ::Timefall::Key::F8
#define TF_KEY_F9              ::Timefall::Key::F9
#define TF_KEY_F10             ::Timefall::Key::F10
#define TF_KEY_F11             ::Timefall::Key::F11
#define TF_KEY_F12             ::Timefall::Key::F12
#define TF_KEY_F13             ::Timefall::Key::F13
#define TF_KEY_F14             ::Timefall::Key::F14
#define TF_KEY_F15             ::Timefall::Key::F15
#define TF_KEY_F16             ::Timefall::Key::F16
#define TF_KEY_F17             ::Timefall::Key::F17
#define TF_KEY_F18             ::Timefall::Key::F18
#define TF_KEY_F19             ::Timefall::Key::F19
#define TF_KEY_F20             ::Timefall::Key::F20
#define TF_KEY_F21             ::Timefall::Key::F21
#define TF_KEY_F22             ::Timefall::Key::F22
#define TF_KEY_F23             ::Timefall::Key::F23
#define TF_KEY_F24             ::Timefall::Key::F24
#define TF_KEY_F25             ::Timefall::Key::F25

/* Keypad */
#define TF_KEY_KP_0            ::Timefall::Key::KP0
#define TF_KEY_KP_1            ::Timefall::Key::KP1
#define TF_KEY_KP_2            ::Timefall::Key::KP2
#define TF_KEY_KP_3            ::Timefall::Key::KP3
#define TF_KEY_KP_4            ::Timefall::Key::KP4
#define TF_KEY_KP_5            ::Timefall::Key::KP5
#define TF_KEY_KP_6            ::Timefall::Key::KP6
#define TF_KEY_KP_7            ::Timefall::Key::KP7
#define TF_KEY_KP_8            ::Timefall::Key::KP8
#define TF_KEY_KP_9            ::Timefall::Key::KP9
#define TF_KEY_KP_DECIMAL      ::Timefall::Key::KPDecimal
#define TF_KEY_KP_DIVIDE       ::Timefall::Key::KPDivide
#define TF_KEY_KP_MULTIPLY     ::Timefall::Key::KPMultiply
#define TF_KEY_KP_SUBTRACT     ::Timefall::Key::KPSubtract
#define TF_KEY_KP_ADD          ::Timefall::Key::KPAdd
#define TF_KEY_KP_ENTER        ::Timefall::Key::KPEnter
#define TF_KEY_KP_EQUAL        ::Timefall::Key::KPEqual

#define TF_KEY_LEFT_SHIFT      ::Timefall::Key::LeftShift
#define TF_KEY_LEFT_CONTROL    ::Timefall::Key::LeftControl
#define TF_KEY_LEFT_ALT        ::Timefall::Key::LeftAlt
#define TF_KEY_LEFT_SUPER      ::Timefall::Key::LeftSuper
#define TF_KEY_RIGHT_SHIFT     ::Timefall::Key::RightShift
#define TF_KEY_RIGHT_CONTROL   ::Timefall::Key::RightControl
#define TF_KEY_RIGHT_ALT       ::Timefall::Key::RightAlt
#define TF_KEY_RIGHT_SUPER     ::Timefall::Key::RightSuper
#define TF_KEY_MENU            ::Timefall::Key::Menu