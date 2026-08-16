#pragma once

#include "Timefall/Renderer/ShaderCompiler.h"

namespace Timefall
{
	class TF_API ShaderCache
	{
	public:
		static std::expected<CompiledModule, CompileError> GetOrCompile(const std::filesystem::path& path);
		static const std::filesystem::path& Directory();
		static const char* SlangVersionTag();
	};
}
