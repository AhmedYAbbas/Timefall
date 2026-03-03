#pragma once

#include <filesystem>

namespace Timefall
{
	class TF_API FileDialogs
	{
	public:
		static std::filesystem::path OpenFile(const char* filter);
		static std::filesystem::path SaveFile(const char* filter);
	};
}
