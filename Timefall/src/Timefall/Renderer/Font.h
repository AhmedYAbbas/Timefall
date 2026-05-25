#pragma once

#include "Timefall/Core/Core.h"

#include <filesystem>

namespace Timefall
{
	class TF_API Font
	{
	public:
		Font(const std::filesystem::path& filepath);
	};
}