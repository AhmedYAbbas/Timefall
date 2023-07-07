#pragma once

#include "Core.h"

namespace Timefall
{
	class TIMEFALL_API Application
	{
	public:
		Application();
		virtual ~Application();

		void Run();
	};

	Application* CreateApplication();
}
