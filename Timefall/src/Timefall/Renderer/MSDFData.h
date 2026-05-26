#pragma once

#include "Timefall/Core/Core.h"

#undef INFINITE
#include "msdf-atlas-gen.h"

#include <vector>

namespace Timefall
{
	struct TF_API MSDFData
	{
		std::vector<msdf_atlas::GlyphGeometry> Glyphs;
		msdf_atlas::FontGeometry FontGeometry;
	};
}