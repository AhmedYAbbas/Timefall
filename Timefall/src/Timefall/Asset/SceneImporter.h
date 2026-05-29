#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Scene/Scene.h"
#include "AssetMetadata.h"

namespace Timefall
{
	class TF_API SceneImporter
	{
	public:
		// AssetMetadata filepath is relative to project asset directory
		static Ref<Scene> ImportScene(AssetHandle handle, const AssetMetadata& metadata);

		// Load from filepath
		static Ref<Scene> LoadScene(const std::filesystem::path& filePath);

		static void SaveScene(const Ref<Scene>& scene, const std::filesystem::path& filePath);
	};
}