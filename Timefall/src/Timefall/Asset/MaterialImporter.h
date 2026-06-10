#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Asset/AssetMetadata.h"
#include "Timefall/Renderer/Material.h"

namespace Timefall
{
	class TF_API MaterialImporter
	{
	public:
		// AssetMetadata filepath is relative to the project's asset directory.
		static Ref<Material> ImportMaterial(AssetHandle handle, const AssetMetadata& metadata);

		// Reads from a full filesystem path.
		static Ref<Material> LoadMaterial(const std::filesystem::path& path);

		// Writes the material to a full filesystem path as .tfmat YAML.
		static void Serialize(const std::filesystem::path& path, const Ref<Material>& material);
	};
}
