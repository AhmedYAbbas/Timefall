#include "tfpch.h"
#include "Timefall/Scene/SceneManager.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Asset/AssetManager.h"
#include "Timefall/Asset/EditorAssetManager.h"
#include "Timefall/Project/Project.h"

namespace Timefall
{
	static Ref<Scene> s_ActiveScene;
	static AssetHandle s_PendingLoadHandle = 0;
	static uint32_t s_ViewportWidth = 0;
	static uint32_t s_ViewportHeight = 0;

	void SceneManager::SetActiveScene(const Ref<Scene>& scene) { s_ActiveScene = scene; }
	Ref<Scene> SceneManager::GetActiveScene() { return s_ActiveScene; }
	void SceneManager::SetViewportSize(uint32_t width, uint32_t height) { s_ViewportWidth = width; s_ViewportHeight = height; }

	void SceneManager::LoadSceneByHandle(AssetHandle handle) { s_PendingLoadHandle = handle; }

	void SceneManager::LoadScene(const std::string& name)
	{
		auto editorAssetManager = Project::GetActive()->GetEditorAssetManager();
		for (const auto& [handle, metadata] : editorAssetManager->GetAssetRegistry())
		{
			if (metadata.Type == AssetType::Scene && metadata.FilePath.stem().string() == name)
			{
				s_PendingLoadHandle = handle;
				return;
			}
		}
		TF_CORE_WARN("SceneManager::LoadScene: no scene asset named '{}'", name);
	}

	bool SceneManager::ProcessPendingLoad()
	{
		if (s_PendingLoadHandle == 0)
			return false;

		AssetHandle handle = s_PendingLoadHandle;
		s_PendingLoadHandle = 0;

		// Load the target first; if it fails, keep the current scene running.
		Ref<Scene> source = AssetManager::GetAsset<Scene>(handle);
		if (!source)
		{
			TF_CORE_ERROR("SceneManager::ProcessPendingLoad: failed to load scene asset {}", (uint64_t)handle);
			return false;
		}

		if (s_ActiveScene)
			s_ActiveScene->OnRuntimeStop();

		Ref<Scene> runtime = Scene::Copy(source);
		runtime->OnViewportResize(s_ViewportWidth, s_ViewportHeight);
		runtime->OnRuntimeStart();
		s_ActiveScene = runtime;
		return true;
	}
}
