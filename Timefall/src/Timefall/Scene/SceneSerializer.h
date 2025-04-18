#pragma once

#include "Scene.h"

#include <filesystem>

namespace Timefall
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		void SerializeText(const std::filesystem::path& filepath);
		void SerializeBinary(const std::filesystem::path& filepath);

		bool DeserializeText(const std::filesystem::path& filepath);
		bool DeserializeBinary(const std::filesystem::path& filepath);

	private:
		Ref<Scene> m_Scene;
	};
}