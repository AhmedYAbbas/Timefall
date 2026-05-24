#pragma once

#include <string>
#include <filesystem>

namespace Timefall
{
	struct TF_API ProjectConfig
	{
		std::string Name = "Untitled";

		std::filesystem::path StartScene;

		std::filesystem::path AssetDirectory;
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

		// TODO: move to asset manager when I have one
		static std::filesystem::path GetAssetFileSystemPath(const std::filesystem::path& relativePath)
		{
			TF_CORE_ASSERT(s_ActiveProject, "Project not initialized!");
			return GetAssetDirectory() / relativePath;
		}

		inline ProjectConfig& GetConfig() { return m_Config; }

		inline static Ref<Project>& GetActive() { return s_ActiveProject; }

		static Ref<Project> New();
		static Ref<Project> Load(const std::filesystem::path& path);
		static bool SaveActive(const std::filesystem::path& path);

	private:
		ProjectConfig m_Config;
		std::filesystem::path m_ProjectDirectory;

		inline static Ref<Project> s_ActiveProject;
	};
}