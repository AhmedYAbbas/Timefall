#include "tfpch.h"
#include "Asset.h"

namespace Timefall
{
    const char* AssetTypeToString(AssetType type)
    {
        switch (type)
        {
		    case AssetType::None:       return "None";
            case AssetType::Scene:      return "Scene";
            case AssetType::Texture2D:  return "Texture2D";
            case AssetType::Material:   return "Material";
        }

		return "Unknown";
    }

    AssetType AssetTypeFromString(const std::string_view& typeString)
    {
		if (typeString == "None")        return AssetType::None;
		if (typeString == "Scene")       return AssetType::Scene;
		if (typeString == "Texture2D")   return AssetType::Texture2D;
		if (typeString == "Material")    return AssetType::Material;

		return AssetType::None;
    }
}