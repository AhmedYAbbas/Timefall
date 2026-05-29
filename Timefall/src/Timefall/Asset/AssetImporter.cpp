#include "tfpch.h"
#include "AssetImporter.h"

#include "TextureImporter.h"
#include "SceneImporter.h"

namespace Timefall
{
	using AssetImportFunction = std::function<Ref<Asset>(AssetHandle, const AssetMetadata&)>;
	static std::unordered_map<AssetType, AssetImportFunction> s_Importers = {
		{ AssetType::Texture2D, TextureImporter::ImportTexture2D },
		{ AssetType::Scene, SceneImporter::ImportScene }
	};

	Ref<Asset> AssetImporter::ImportAsset(AssetHandle handle, const AssetMetadata& metadata)
	{
		if (!s_Importers.contains(metadata.Type))
		{
			TF_CORE_ERROR("No importer found for asset type: {0}", (uint32_t)metadata.Type);
			return nullptr;
		}

		return s_Importers.at(metadata.Type)(handle, metadata);
	}
}