#pragma once

#include <filesystem>

namespace Timefall
{
	class FileDialogs
	{
	public:
		static std::filesystem::path OpenFile(const char* filter);
		static std::filesystem::path SaveFile(const char* filter);
	};
}
