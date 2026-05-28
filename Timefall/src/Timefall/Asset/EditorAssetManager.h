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

		void ImportAsset(const std::filesystem::path& filePath);

		const AssetMetadata& GetMetadata(AssetHandle handle) const;
		const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }

		void SerializeAssetRegistry() const;
		bool DeserializeAssetRegistry();

	private:
		AssetRegistry m_AssetRegistry;
		AssetMap m_LoadedAssets;

		// TODO: memory-only assets
	};
}
