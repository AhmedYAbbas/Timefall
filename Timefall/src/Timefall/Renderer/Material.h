#pragma once

#include "Timefall/Asset/Asset.h"

#include <glm/glm.hpp>

#include <string_view>

namespace Timefall
{
	// How a material's alpha is interpreted (mirrors glTF alphaMode).
	//   Opaque - alpha ignored, surface is solid.
	//   Mask   - alpha is a cutout: discard fragments below AlphaCutoff, rest is solid.
	//   Blend  - alpha is translucency: blend the fragment over what's behind it.
	enum class AlphaMode { Opaque = 0, Mask = 1, Blend = 2 };

	constexpr std::string_view AlphaModeToString(AlphaMode mode)
	{
		switch (mode)
		{
			case AlphaMode::Mask:  return "Mask";
			case AlphaMode::Blend: return "Blend";
			default:               return "Opaque";
		}
	}

	constexpr AlphaMode AlphaModeFromString(std::string_view s)
	{
		if (s == "Mask")  return AlphaMode::Mask;
		if (s == "Blend") return AlphaMode::Blend;
		return AlphaMode::Opaque;
	}

	// A standalone, editable material asset (.tfmat). Maps of 0 mean "no map — use the color".
	class TF_API Material : public Asset
	{
	public:
		glm::vec3 BaseColor{ 1.0f, 1.0f, 1.0f };  // albedo; sRGB in picker, linearized on bind
		float     Metallic = 0.0f;
		float     Roughness = 1.0f;
		float     NormalStrength = 1.0f;

		AlphaMode Alpha = AlphaMode::Opaque;
		float     Opacity = 1.0f;       // base-color alpha factor (glTF baseColorFactor.a)
		float     AlphaCutoff = 0.5f;   // Mask threshold; unused for Opaque/Blend

		AssetHandle BaseColorMap = 0;  // sRGB
		AssetHandle NormalMap = 0;     // linear
		AssetHandle MetallicMap = 0;   // linear, sampled .b (glTF metal-rough packing)
		AssetHandle RoughnessMap = 0;  // linear, sampled .g (glTF metal-rough packing)
		AssetHandle AOMap = 0;         // linear, sampled .r

		glm::vec3 Emissive{ 0.0f, 0.0f, 0.0f };  // sRGB in picker, linearized on bind
		float     EmissiveIntensity = 1.0f;
		AssetHandle EmissiveMap = 0;             // sRGB

		static AssetType GetStaticType() { return AssetType::Material; }
		virtual AssetType GetType() const override { return GetStaticType(); }
	};
}
