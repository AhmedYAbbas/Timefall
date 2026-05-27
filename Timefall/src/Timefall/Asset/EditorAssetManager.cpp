#include "tfpch.h"
#include "EditorAssetManager.h"

namespace Timefall
{
	Ref<Asset> EditorAssetManager::GetAsset(AssetHandle handle) const
	{
		if (!IsAssetHandleValid(handle))
		{
			TF_CORE_ERROR("Invalid asset handle: {0}", (uint64_t)handle);
			return nullptr;
		}

		Ref<Asset> asset;
		if (IsAssetLoaded(handle))
		{
			asset = m_LoadedAssets.at(handle);
		}
		else
		{
			//const AssetMetadata& metadata = GetMetadata(handle);
			// asset = AssetImporter::ImportAsset(metadata);
			// if (!asset) {} // import failed
		}

		return asset;
	}

	bool EditorAssetManager::IsAssetHandleValid(AssetHandle handle) const
	{
		return (uint64_t)handle != 0 && m_AssetRegistry.contains(handle);
	}

	bool EditorAssetManager::IsAssetLoaded(AssetHandle handle) const
	{
		return m_LoadedAssets.contains(handle);
	}

	const AssetMetadata& EditorAssetManager::GetAssetMetadata(AssetHandle handle) const
	{
		static AssetMetadata s_NullMetadata;
		if (!m_AssetRegistry.contains(handle))
			return s_NullMetadata;

		return m_AssetRegistry.at(handle);
	}
}