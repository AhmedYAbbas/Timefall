#pragma once

#include "Timefall/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Timefall
{
	// A standalone, editable material asset (.tfmat). Maps of 0 mean "no map — use the color".
	class TF_API Material : public Asset
	{
	public:
		glm::vec3 DiffuseColor{ 1.0f, 1.0f, 1.0f };
		glm::vec3 SpecularColor{ 1.0f, 1.0f, 1.0f };
		float     Shininess = 32.0f;

		AssetHandle DiffuseMap = 0;
		AssetHandle SpecularMap = 0;
		AssetHandle NormalMap = 0;

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetType() const override { return GetStaticType(); }
	};
}
