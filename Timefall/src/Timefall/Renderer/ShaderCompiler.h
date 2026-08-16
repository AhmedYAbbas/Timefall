#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Renderer/ShaderReflection.h"

namespace Timefall
{
	struct CompileError
	{
		std::string Message;
		std::string File;
		int Line = 0;
	};

	struct CompiledEntryPoint
	{
		std::string Name;
		ShaderStage Stage = ShaderStage::Vertex;
		std::vector<uint32_t> SpirV;
	};

	struct CompiledModule
	{
		std::vector<CompiledEntryPoint> EntryPoints;
		ShaderReflection Reflection;
		std::vector<std::filesystem::path> Dependencies;
	};

	class TF_API ShaderCompiler
	{
	public:
		static bool IsAvailable();
		static std::expected<CompiledModule, CompileError> Compile(const std::filesystem::path& path);

	public:
		static constexpr const char* TARGET_PROFILE = "spirv_1_6";
	};
}
