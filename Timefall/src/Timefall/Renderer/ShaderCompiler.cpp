#include "tfpch.h"
#include "ShaderCompiler.h"

#ifdef TF_SHADER_RUNTIME_COMPILER
#include <slang-com-ptr.h>
#include <slang.h>
#endif

#include <charconv>

namespace Timefall
{
	static constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
	static constexpr uint64_t FNV_PRIME = 1099511628211ull;

	static void HashBytes(uint64_t& hash, const void* data, size_t size)
	{
		const auto* bytes = (const uint8_t*)data;
		for (size_t i = 0; i < size; i++)
		{
			hash ^= bytes[i];
			hash *= FNV_PRIME;
		}
	}

	uint64_t ComputeReflectionDigest(const ShaderReflection& reflection)
	{
		uint64_t bindings = 0;
		for (const auto& binding : reflection.Bindings)
		{
			uint64_t one = FNV_OFFSET;
			HashBytes(one, binding.Name.data(), binding.Name.size());
			HashBytes(one, &binding.Set, sizeof(binding.Set));
			HashBytes(one, &binding.Binding, sizeof(binding.Binding));
			HashBytes(one, &binding.Count, sizeof(binding.Count));
			HashBytes(one, &binding.Type, sizeof(binding.Type));
			bindings ^= one;
		}

		uint64_t hash = FNV_OFFSET;
		HashBytes(hash, &bindings, sizeof(bindings));
		HashBytes(hash, &reflection.PushConstantSize, sizeof(reflection.PushConstantSize));
		HashBytes(hash, &reflection.StageMask, sizeof(reflection.StageMask));
		for (const auto& input : reflection.VertexInputs)
		{
			HashBytes(hash, input.Name.data(), input.Name.size());
			HashBytes(hash, &input.Location, sizeof(input.Location));
		}

		return hash;
	}

#ifdef TF_SHADER_RUNTIME_COMPILER

	static Slang::ComPtr<slang::IGlobalSession> s_GlobalSession;
	static bool s_ProbeFailed = false;

	static bool EnsureLoaded()
	{
		if (s_GlobalSession)
			return true;

		if (s_ProbeFailed)
			return false;

		if (!LoadLibraryW(L"slang.dll"))
		{
			TF_CORE_ERROR("slang.dll not found - shader compilation unavailable");
			s_ProbeFailed = true;
			return false;
		}

		if (SLANG_FAILED(slang::createGlobalSession(s_GlobalSession.writeRef())))
		{
			TF_CORE_ERROR("slang::createGlobalSession failed");
			s_ProbeFailed = true;
			return false;
		}

		return true;
	}

	static CompileError ErrorFromBlob(slang::IBlob* diagnostics, const std::filesystem::path& path)
	{
		CompileError error;
		error.File = path.string();
		error.Message = diagnostics && diagnostics->getBufferPointer() ? std::string((const char*)diagnostics->getBufferPointer())
																	   : std::string("unknown Slang error");

		if (size_t open = error.Message.find('('); open != std::string::npos)
		{
			if (size_t close = error.Message.find(')', open); close != std::string::npos)
			{
				std::string_view text(error.Message.data() + open + 1, close - open - 1);
				if (size_t comma = text.find(','); comma != std::string_view::npos)
					text = text.substr(0, comma);

				std::from_chars(text.data(), text.data() + text.size(), error.Line);
			}
		}

		return error;
	}

	static ShaderStage FromSlangStage(SlangStage stage)
	{
		return stage == SLANG_STAGE_VERTEX ? ShaderStage::Vertex : ShaderStage::Fragment;
	}

	static bool ClassifyBinding(
		slang::TypeLayoutReflection* typeLayout, ShaderBindingType& outType, uint32_t& outCount, std::string& outReason)
	{
		outCount = 1;

		if (typeLayout->getKind() == slang::TypeReflection::Kind::Array)
		{
			const size_t elements = typeLayout->getElementCount();
			outCount = (uint32_t)elements;
			typeLayout = typeLayout->getElementTypeLayout();
		}

		switch (typeLayout->getKind())
		{
			case slang::TypeReflection::Kind::ConstantBuffer: outType = ShaderBindingType::UniformBuffer; return true;

			case slang::TypeReflection::Kind::SamplerState: outType = ShaderBindingType::Sampler; return true;

			case slang::TypeReflection::Kind::Resource:
			{
				const SlangResourceShape shape =
					(SlangResourceShape)(typeLayout->getType()->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK);

				if (shape == SLANG_STRUCTURED_BUFFER || shape == SLANG_BYTE_ADDRESS_BUFFER)
				{
					outType = ShaderBindingType::StorageBuffer;
					return true;
				}

				if (typeLayout->getType()->getResourceAccess() != SLANG_RESOURCE_ACCESS_READ)
				{
					outReason = "Writable resources are not supported until compute lands";
					return false;
				}

				outType = ShaderBindingType::SampledImage;
				return true;
			}

			default:
				outReason = "Unsupported resource kind - declare a ConstantBuffer, Texture, SamplerState or StructuredBuffer";
				return false;
		}
	}

	static std::expected<void, CompileError> ReflectModule(
		slang::ProgramLayout* layout, const std::filesystem::path& path, ShaderReflection& out)
	{
		const unsigned int parameterCount = layout->getParameterCount();
		for (unsigned int i = 0; i < parameterCount; i++)
		{
			slang::VariableLayoutReflection* parameter = layout->getParameterByIndex(i);
			const std::string name = parameter->getName() ? parameter->getName() : "<unnamed>";

			switch (parameter->getCategory())
			{
				case slang::ParameterCategory::Uniform:
					return std::unexpected(CompileError{
						std::format("'{}' is a loose uniform; use ConstantBuffer<T> or [[vk::push_constant]]", name), path.string(), 0});

				case slang::ParameterCategory::PushConstantBuffer:
					out.PushConstantSize =
						(uint32_t)parameter->getTypeLayout()->getElementTypeLayout()->getSize(slang::ParameterCategory::Uniform);
					break;

				case slang::ParameterCategory::DescriptorTableSlot:
				{
					ShaderBinding binding;
					binding.Name = name;
					binding.Set = (uint32_t)parameter->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot);
					binding.Binding = (uint32_t)parameter->getOffset(slang::ParameterCategory::DescriptorTableSlot);

					std::string reason;
					if (!ClassifyBinding(parameter->getTypeLayout(), binding.Type, binding.Count, reason))
						return std::unexpected(CompileError{std::format("'{}': {}", name, reason), path.string(), 0});

					out.Bindings.push_back(std::move(binding));
					break;
				}

				default: break;
			}
		}

		// Vertex inputs come from the vertex entry point's parameter list, in declaration order.
		const SlangInt entryPointCount = layout->getEntryPointCount();
		for (SlangInt i = 0; i < entryPointCount; i++)
		{
			slang::EntryPointReflection* entryPoint = layout->getEntryPointByIndex(i);
			out.StageMask |= StageBit(FromSlangStage(entryPoint->getStage()));

			if (entryPoint->getStage() != SLANG_STAGE_VERTEX)
				continue;

			const unsigned int inputCount = entryPoint->getParameterCount();
			uint32_t location = 0;
			for (unsigned int p = 0; p < inputCount; p++)
			{
				slang::VariableLayoutReflection* input = entryPoint->getParameterByIndex(p);
				if (input->getCategory() != slang::ParameterCategory::VaryingInput)
					continue; // SV_VertexID and friends are system values, not vertex attributes

				out.VertexInputs.push_back({input->getName() ? input->getName() : "<unnamed>", location++});
			}
		}

		out.Digest = ComputeReflectionDigest(out);
		return {};
	}

#endif
}

namespace Timefall
{
#ifdef TF_SHADER_RUNTIME_COMPILER

	bool ShaderCompiler::IsAvailable()
	{
		return EnsureLoaded();
	}

	std::expected<CompiledModule, CompileError> ShaderCompiler::Compile(const std::filesystem::path& path)
	{
		TF_PROFILE_FUNCTION();

		if (!EnsureLoaded())
			return std::unexpected(CompileError{"slang.dll unavailable", path.string(), 0});

		slang::TargetDesc target{};
		target.format = SLANG_SPIRV;
		target.profile = s_GlobalSession->findProfile(TARGET_PROFILE);

		const std::array options{
			slang::CompilerOptionEntry{slang::CompilerOptionName::MatrixLayoutColumn, {slang::CompilerOptionValueKind::Int, 1, 0}},
			slang::CompilerOptionEntry{slang::CompilerOptionName::EmitSpirvDirectly, {slang::CompilerOptionValueKind::Int, 1, 0}},
			// Without this, Slang names every per-stage SPIR-V entry point "main" instead of the
			// declared name, and vkCreateGraphicsPipelines' pName lookup (reflection-driven) fails.
			slang::CompilerOptionEntry{slang::CompilerOptionName::VulkanUseEntryPointName, {slang::CompilerOptionValueKind::Int, 1, 0}}};

		const std::string searchPath = path.parent_path().string();
		const char* searchPaths[] = {searchPath.c_str()};

		slang::SessionDesc sessionDesc{};
		sessionDesc.targets = &target;
		sessionDesc.targetCount = 1;
		sessionDesc.searchPaths = searchPaths;
		sessionDesc.searchPathCount = 1;
		sessionDesc.compilerOptionEntries = const_cast<slang::CompilerOptionEntry*>(options.data());
		sessionDesc.compilerOptionEntryCount = (uint32_t)options.size();

		Slang::ComPtr<slang::ISession> session;
		if (SLANG_FAILED(s_GlobalSession->createSession(sessionDesc, session.writeRef())))
			return std::unexpected(CompileError{"Slang createSession failed", path.string(), 0});

		Slang::ComPtr<slang::IBlob> diagnostics;
		const std::string moduleName = path.stem().string();
		slang::IModule* module = session->loadModule(moduleName.c_str(), diagnostics.writeRef());
		if (!module)
			return std::unexpected(ErrorFromBlob(diagnostics, path));

		const SlangInt32 definedCount = module->getDefinedEntryPointCount();
		if (definedCount == 0)
			return std::unexpected(CompileError{"Module declares no [shader(...)] entry points", path.string(), 0});

		std::vector<Slang::ComPtr<slang::IEntryPoint>> entryPoints((size_t)definedCount);
		std::vector<slang::IComponentType*> components;
		components.push_back(module);

		for (SlangInt32 i = 0; i < definedCount; i++)
		{
			if (SLANG_FAILED(module->getDefinedEntryPoint(i, entryPoints[(size_t)i].writeRef())))
				return std::unexpected(CompileError{std::format("getDefinedEntryPoint({}) failed", i), path.string(), 0});

			components.push_back(entryPoints[(size_t)i].get());
		}

		Slang::ComPtr<slang::IComponentType> program;
		if (SLANG_FAILED(session->createCompositeComponentType(
				components.data(), (SlangInt)components.size(), program.writeRef(), diagnostics.writeRef())))
			return std::unexpected(ErrorFromBlob(diagnostics, path));

		Slang::ComPtr<slang::IComponentType> linked;
		if (SLANG_FAILED(program->link(linked.writeRef(), diagnostics.writeRef())))
			return std::unexpected(ErrorFromBlob(diagnostics, path));

		slang::ProgramLayout* layout = linked->getLayout(0, diagnostics.writeRef());
		if (!layout)
			return std::unexpected(ErrorFromBlob(diagnostics, path));

		CompiledModule result;
		if (auto reflected = ReflectModule(layout, path, result.Reflection); !reflected)
			return std::unexpected(reflected.error());

		result.EntryPoints.reserve((size_t)definedCount);
		for (SlangInt32 i = 0; i < definedCount; i++)
		{
			Slang::ComPtr<slang::IBlob> code;
			if (SLANG_FAILED(linked->getEntryPointCode(i, 0, code.writeRef(), diagnostics.writeRef())))
				return std::unexpected(ErrorFromBlob(diagnostics, path));

			slang::EntryPointReflection* entryPoint = layout->getEntryPointByIndex(i);

			CompiledEntryPoint compiled;
			compiled.Name = entryPoint->getName() ? entryPoint->getName() : "";
			compiled.Stage = FromSlangStage(entryPoint->getStage());
			compiled.SpirV.resize(code->getBufferSize() / sizeof(uint32_t));
			std::memcpy(compiled.SpirV.data(), code->getBufferPointer(), compiled.SpirV.size() * sizeof(uint32_t));

			result.EntryPoints.push_back(std::move(compiled));
		}

		const SlangInt32 depCount = module->getDependencyFileCount();
		result.Dependencies.reserve((size_t)depCount);
		for (SlangInt32 i = 0; i < depCount; i++)
			result.Dependencies.emplace_back(module->getDependencyFilePath(i));

		return result;
	}

#else

	bool ShaderCompiler::IsAvailable()
	{
		return false;
	}

	std::expected<CompiledModule, CompileError> ShaderCompiler::Compile(const std::filesystem::path& path)
	{
		return std::unexpected(CompileError{"Built without TF_SHADER_RUNTIME_COMPILER", path.string(), 0});
	}

#endif
}
