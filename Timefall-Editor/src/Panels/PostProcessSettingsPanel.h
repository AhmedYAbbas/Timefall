#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	class Scene;

	class PostProcessSettingsPanel
	{
	public:
		void OnImGuiRender(const Ref<Scene>& sceneContext);
	};
}
