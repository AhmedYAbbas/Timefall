#pragma once

#include "Timefall/Core/Core.h"
#include "Asset.h"

#include <filesystem>

namespace Timefall
{
	struct TF_API AssetMetadata
	{
		AssetType Type = AssetType::None;
		std::filesystem::path FilePath;

		operator bool() const { return Type != AssetType::None; }
	};
}