#pragma once

#include "Timefall/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Timefall
{
	// A standalone, editable material asset (.tfmat). Maps of 0 mean "no map — use the color".
	class TF_API Material : public Asset
	{
	public:
		glm::vec3 BaseColor{ 1.0f, 1.0f, 1.0f };  // albedo; sRGB in picker, linearized on bind
		float     Metallic = 0.0f;
		float     Roughness = 1.0f;
		float     NormalStrength = 1.0f;

		AssetHandle BaseColorMap = 0;  // sRGB
		AssetHandle NormalMap = 0;     // linear
		AssetHandle MetallicMap = 0;   // linear, sampled .r
		AssetHandle RoughnessMap = 0;  // linear, sampled .r
		AssetHandle AOMap = 0;         // linear, sampled .r

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetType() const override { return GetStaticType(); }
	};
}
