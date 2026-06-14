#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Core/UUID.h"

namespace Timefall
{
	using AssetHandle = UUID;

	enum class TF_API AssetType : uint16_t
	{
		None = 0,
		Scene,
		Texture2D,
		Material,
		Mesh
	};

	const char* AssetTypeToString(AssetType type);
	AssetType AssetTypeFromString(const std::string_view& typeString);

	class TF_API Asset
	{
	public:
		virtual AssetType GetType() const = 0;

	public:
		AssetHandle Handle; // Generates handle
	};
}