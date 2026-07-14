#pragma once

#include "Timefall/Core/Core.h"
#include "AssetManagerBase.h"

#include "Timefall/Project/Project.h"

namespace Timefall
{
	class TF_API AssetManager
	{
	public:
		template <typename T> static Ref<T> GetAsset(AssetHandle handle)
		{
			Ref<Asset> asset = Project::GetActive()->GetAssetManager()->GetAsset(handle);
			return std::static_pointer_cast<T>(asset);
		}

		static bool IsAssetHandleValid(AssetHandle handle) { return Project::GetActive()->GetAssetManager()->IsAssetHandleValid(handle); }

		static bool IsAssetLoaded(AssetHandle handle) { return Project::GetActive()->GetAssetManager()->IsAssetLoaded(handle); }

		static AssetType GetAssetType(AssetHandle handle) { return Project::GetActive()->GetAssetManager()->GetAssetType(handle); }
	};
}