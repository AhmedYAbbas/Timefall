#pragma once

#include "Timefall/Core/Core.h"

#include "Asset.h"

namespace Timefall
{
	using AssetMap = std::unordered_map<AssetHandle, Ref<Asset>>;

	class TF_API AssetManagerBase
	{
	public:
		virtual Ref<Asset> GetAsset(AssetHandle handle) = 0;

		virtual bool IsAssetHandleValid(AssetHandle handle) const = 0;
		virtual bool IsAssetLoaded(AssetHandle handle) const = 0;
		virtual AssetType GetAssetType(AssetHandle handle) const = 0;
	};
}