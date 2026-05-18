#pragma once

#include "Timefall/Core/Buffer.h"
#include <filesystem>

namespace Timefall
{

	class FileSystem
	{
	public:
		// TODO: move to FileSystem class
		static Buffer ReadFileBinary(const std::filesystem::path& filepath);
	};
}