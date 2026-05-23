#pragma once

#include "Timefall/Core/Buffer.h"
#include "Timefall/Core/Core.h"

#include <filesystem>

namespace Timefall
{

	class TF_API FileSystem
	{
	public:
		// TODO: move to FileSystem class
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
	};
}