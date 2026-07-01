#pragma once

#include <cstdint>

namespace Timefall
{
	// Order MUST match the switch in Renderer3D_Resolve.glsl.
	enum class ToneMapOperator : uint32_t
	{
		None = 0,
		Reinhard,
		ReinhardExtended,
		Hable,
		ACESNarkowicz,
		ACESHill,
		AgX,
		KhronosPBRNeutral
	};

	struct PostProcessSettings
	{
		ToneMapOperator Operator           = ToneMapOperator::ACESHill;
		float           ExposureEV         = 0.0f;
		float           ReinhardWhitePoint = 4.0f;
	};
}
