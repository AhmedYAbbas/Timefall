#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	class TF_API RenderDocCapture
	{
	public:
		static bool IsAttached();

		static void TriggerCapture();
	};
}
