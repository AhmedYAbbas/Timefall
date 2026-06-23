#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	class Scene;

	// Dedicated panel for the scene's global directional-shadow settings (Scene::GetShadowSettings).
	// Edits the struct in place; the renderer picks the changes up the next frame in RenderScene.
	class ShadowSettingsPanel
	{
	public:
		ShadowSettingsPanel() = default;

		void OnImGuiRender(const Ref<Scene>& sceneContext);
	};
}
