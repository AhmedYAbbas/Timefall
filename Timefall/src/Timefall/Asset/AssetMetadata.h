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
		bool MemoryOnly = false;   // built-in/runtime assets not written to the on-disk registry

		operator bool() const { return Type != AssetType::None; }
	};
}