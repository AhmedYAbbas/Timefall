#pragma once

#include <cstdint>

namespace Timefall
{
	enum class ShadowCullMode : uint32_t { Back = 0, Front, None };

	struct ShadowSettings
	{
		static constexpr uint32_t MaxCascades = 4;

		uint32_t CascadeCount = 4;
		uint32_t ShadowMapResolution = 2048;
		float MaxShadowDistance = 100.0f;
		float SplitLambda = 0.85f;
		float CascadeBlend = 0.1f;
		uint32_t BlockerSearchSamples = 16;
		uint32_t PCFSamples = 16;
		bool SoftShadows = true;
		bool VisualizeCascades = false;
		uint32_t SpotShadowResolution = 1024;
		uint32_t PointShadowResolution = 512;
		ShadowCullMode CullMode = ShadowCullMode::Back;
	};
}
