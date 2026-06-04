#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Asset/Asset.h" // AssetHandle

#include <string>

namespace Timefall
{
	class Scene;

	// Owns the active runtime scene and a deferred scene-load request. The host (editor or a future
	// standalone runtime) drives ProcessPendingLoad() at a safe point each frame.
	class TF_API SceneManager
	{
	public:
		static void SetActiveScene(const Ref<Scene>& scene);
		static Ref<Scene> GetActiveScene();

		// Records a deferred load (no swap). Name = a scene asset's file stem, e.g. "Game".
		static void LoadScene(const std::string& name);
		static void LoadSceneByHandle(AssetHandle handle);

		// Performs the deferred swap if one is pending; returns true if the active scene changed.
		static bool ProcessPendingLoad();

		static void SetViewportSize(uint32_t width, uint32_t height);
	};
}
