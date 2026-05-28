#pragma once

#include "Timefall/Core/Core.h"
#include "AssetMetadata.h"

namespace Timefall
{
	class TF_API AssetImporter
	{
	public:
		static Ref<Asset> ImportAsset(AssetHandle handle, const AssetMetadata& metadata);
	};
}