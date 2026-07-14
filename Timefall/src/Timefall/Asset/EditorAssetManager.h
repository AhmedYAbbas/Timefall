#pragma once

#include "AssetManagerBase.h"
#include "AssetMetadata.h"

namespace Timefall
{
	using AssetRegistry = std::map<AssetHandle, AssetMetadata>;

	class TF_API EditorAssetManager : public AssetManagerBase
	{
	public:
		virtual Ref<Asset> GetAsset(AssetHandle handle) override;

		virtual bool IsAssetHandleValid(AssetHandle handle) const override;
		virtual bool IsAssetLoaded(AssetHandle handle) const override;
		virtual AssetType GetAssetType(AssetHandle handle) const override;

		AssetHandle ImportAsset(const std::filesystem::path& filePath);

		// Register a disk-backed asset whose instance is already in memory (built once during model
		// import). Re-imported from FilePath on reload. Returns 0 if asset is null.
		AssetHandle ImportLoadedAsset(const std::filesystem::path& filePath, const Ref<Asset>& asset);

		// Seed an in-memory asset (built-in primitives, runtime-created) under a fixed handle.
		// Visible to the registry/content browser but never serialized to disk.
		void AddMemoryOnlyAsset(AssetHandle handle, const Ref<Asset>& asset, const std::filesystem::path& virtualPath, AssetType type);

		const AssetMetadata& GetMetadata(AssetHandle handle) const;
		const AssetRegistry& GetAssetRegistry() const { return m_AssetRegistry; }
		const std::filesystem::path& GetFilePath(AssetHandle handle) const;

		void SerializeAssetRegistry() const;
		bool DeserializeAssetRegistry();

	private:
		AssetRegistry m_AssetRegistry;
		AssetMap m_LoadedAssets;
	};
}
