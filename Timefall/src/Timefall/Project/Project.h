#pragma once

#include "Timefall/Asset/EditorAssetManager.h"
#include "Timefall/Asset/RuntimeAssetManager.h"

#include <string>
#include <filesystem>

namespace Timefall
{
	struct TF_API ProjectConfig
	{
		std::string Name = "Untitled";

		AssetHandle StartScene;

		std::filesystem::path AssetDirectory;
		std::filesystem::path AssetRegistryPath; // Relative to AssetDirectory
		std::filesystem::path ScriptModulePath;
	};

	class TF_API Project
	{
	public:
		static const std::filesystem::path& GetProjectDirectory()
		{
			TF_CORE_ASSERT(s_ActiveProject, "Project not initialized!");
			return s_ActiveProject->m_ProjectDirectory;
		}

		static std::filesystem::path GetAssetDirectory()
		{
			TF_CORE_ASSERT(s_ActiveProject, "Project not initialized!");
			return GetProjectDirectory() / s_ActiveProject->m_Config.AssetDirectory;
		}

		static std::filesystem::path GetAssetRegistryPath()
		{
			TF_CORE_ASSERT(s_ActiveProject, "Project not initialized!");
			return GetAssetDirectory() / s_ActiveProject->m_Config.AssetRegistryPath;
		}

		// TODO: move to asset manager when I have one
		static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& relativePath)
		{
			TF_CORE_ASSERT(s_ActiveProject, "Project not initialized!");
			return GetAssetDirectory() / relativePath;
		}

		inline ProjectConfig& GetConfig() { return m_Config; }

		inline static Ref<Project>& GetActive() { return s_ActiveProject; }
		Ref<AssetManagerBase> GetAssetManager() { return m_AssetManager; }
		Ref<RuntimeAssetManager> GetRuntimeAssetManager() { return std::static_pointer_cast<RuntimeAssetManager>(m_AssetManager); }
		Ref<EditorAssetManager> GetEditorAssetManager() { return std::static_pointer_cast<EditorAssetManager>(m_AssetManager); }

		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool SaveActive(const std::filesystem::path& path);

	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;
		Ref<AssetManagerBase> m_AssetManager;

		inline static Ref<Project> s_ActiveProject;
	};
}