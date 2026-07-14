#pragma once

#include "Project.h"
#include <filesystem>

namespace Timefall
{
	class TF_API ProjectSerializer
	{
	public:
		ProjectSerializer(const Ref<Project>& project);

		bool Serialize(const std::filesystem::path& filepath);
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		Ref<Project> m_Project;
	};
}