#pragma once

#include "AssetManagerBase.h"
#include "AssetMetadata.h"

namespace Timefall
{
	using AssetRegistry = std::map<AssetHandle, AssetMetadata>;

	class TF_API EditorAssetManager : public AssetManagerBase
	{
	public:
		virtual Ref<Asset> GetAsset(AssetHandle handle) const override;

		virtual bool IsAssetHandleValid(AssetHandle handle) const override;
		virtual bool IsAssetLoaded(AssetHandle handle) const override;

		const AssetMetadata& GetAssetMetadata(AssetHandle handle) const;

	private:
		AssetRegistry m_AssetRegistry;
		AssetMap m_LoadedAssets;

		// TODO: memory-only assets
	};
}
