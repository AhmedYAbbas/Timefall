#include "tfpch.h"
#include "ShaderCache.h"

namespace Timefall
{
	static constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
	static constexpr uint64_t FNV_PRIME = 1099511628211ull;
	static constexpr uint32_t META_VERSION = 2; // bumped: entry points now keep their declared SPIR-V names

	static constexpr const char* OPTIMIZATION_TAG = "O-default";

	static void HashBytes(uint64_t& hash, const void* data, size_t size)
	{
		const auto* bytes = (const uint8_t*)data;
		for (size_t i = 0; i < size; i++)
		{
			hash ^= bytes[i];
			hash *= FNV_PRIME;
		}
	}

	static void HashString(uint64_t& hash, std::string_view text)
	{
		HashBytes(hash, text.data(), text.size());
	}

	static bool ReadWholeFile(const std::filesystem::path& path, std::string& out)
	{
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in)
			return false;

		in.seekg(0, std::ios::end);
		const std::streamoff size = in.tellg();
		if (size < 0)
			return false;

		out.resize((size_t)size);
		in.seekg(0, std::ios::beg);
		in.read(out.data(), size);
		return true;
	}

	static bool ReadSpirv(const std::filesystem::path& path, std::vector<uint32_t>& out)
	{
		std::string bytes;
		if (!ReadWholeFile(path, bytes) || bytes.empty() || bytes.size() % sizeof(uint32_t) != 0)
			return false;

		out.resize(bytes.size() / sizeof(uint32_t));
		std::memcpy(out.data(), bytes.data(), bytes.size());
		return true;
	}

	// Key = content of every dependency + target profile + Slang version + optimization level + meta format version
	// Returns false if any dependency is unreadable, which forces a recompile
	static bool ComputeKey(const std::vector<std::filesystem::path>& dependencies, uint64_t& outHash)
	{
		uint64_t hash = FNV_OFFSET;
		std::string contents;
		for (const auto& dependency : dependencies)
		{
			if (!ReadWholeFile(dependency, contents))
				return false;

			HashString(hash, contents);
		}

		HashString(hash, ShaderCompiler::TARGET_PROFILE);
		HashString(hash, ShaderCache::SlangVersionTag());
		HashString(hash, OPTIMIZATION_TAG);
		HashBytes(hash, &META_VERSION, sizeof(META_VERSION));

		outHash = hash;
		return true;
	}

	static std::filesystem::path MetaPath(const std::filesystem::path& source)
	{
		return ShaderCache::Directory() / std::format("{}.meta", source.stem().string());
	}

	static std::filesystem::path SpirvPath(const std::filesystem::path& source, const std::string& entryPoint, uint64_t hash)
	{
		return ShaderCache::Directory() / std::format("{}.{}.{:016x}.spv", source.stem().string(), entryPoint, hash);
	}

	// Sidecar layout, whitespace-separated so it stays readable when debugging a stale hit:
	//   <entryCount> <bindingCount> <inputCount> <pushSize> <stageMask> <digest> <depCount>
	//   <name> <stage>            x entryCount
	//   <name> <set> <binding> <count> <type>   x bindingCount
	//   <name> <location>         x inputCount
	//   <path>                    x depCount, one per line
	static bool ReadMeta(const std::filesystem::path& path, CompiledModule& out)
	{
		std::ifstream in(path);
		if (!in)
			return false;

		size_t entryCount = 0, bindingCount = 0, inputCount = 0, depCount = 0;
		uint32_t stageMask = 0;
		if (!(in >> entryCount >> bindingCount >> inputCount >> out.Reflection.PushConstantSize >> stageMask >> out.Reflection.Digest
				>> depCount))
			return false;

		out.Reflection.StageMask = (uint8_t)stageMask;

		out.EntryPoints.resize(entryCount);
		for (auto& entry : out.EntryPoints)
		{
			uint32_t stage = 0;
			if (!(in >> entry.Name >> stage))
				return false;

			entry.Stage = (ShaderStage)stage;
		}

		out.Reflection.Bindings.resize(bindingCount);
		for (auto& binding : out.Reflection.Bindings)
		{
			uint32_t type = 0;
			if (!(in >> binding.Name >> binding.Set >> binding.Binding >> binding.Count >> type))
				return false;

			binding.Type = (ShaderBindingType)type;
		}

		out.Reflection.VertexInputs.resize(inputCount);
		for (auto& input : out.Reflection.VertexInputs)
			if (!(in >> input.Name >> input.Location))
				return false;

		out.Dependencies.clear();
		out.Dependencies.reserve(depCount);

		std::string line;
		std::getline(in, line);
		for (size_t i = 0; i < depCount && std::getline(in, line); i++)
			if (!line.empty())
				out.Dependencies.emplace_back(line);

		return out.Dependencies.size() == depCount;
	}

	static void WriteMeta(const std::filesystem::path& path, const CompiledModule& module)
	{
		std::ofstream out(path, std::ios::trunc);
		out << module.EntryPoints.size() << ' ' << module.Reflection.Bindings.size() << ' ' << module.Reflection.VertexInputs.size() << ' '
			<< module.Reflection.PushConstantSize << ' ' << (uint32_t)module.Reflection.StageMask << ' ' << module.Reflection.Digest << ' '
			<< module.Dependencies.size() << '\n';

		for (const auto& entry : module.EntryPoints)
			out << entry.Name << ' ' << (uint32_t)entry.Stage << '\n';

		for (const auto& binding : module.Reflection.Bindings)
			out << binding.Name << ' ' << binding.Set << ' ' << binding.Binding << ' ' << binding.Count << ' ' << (uint32_t)binding.Type
				<< '\n';

		for (const auto& input : module.Reflection.VertexInputs)
			out << input.Name << ' ' << input.Location << '\n';

		for (const auto& dependency : module.Dependencies)
			out << dependency.string() << '\n';
	}

	const std::filesystem::path& ShaderCache::Directory()
	{
		static const std::filesystem::path directory = "Cache/Shaders";
		return directory;
	}

	const char* ShaderCache::SlangVersionTag()
	{
		return "slang-2026.8";
	}

	std::expected<CompiledModule, CompileError> ShaderCache::GetOrCompile(const std::filesystem::path& path)
	{
		TF_PROFILE_FUNCTION();

		std::error_code ec;
		std::filesystem::create_directories(Directory(), ec);

		const std::filesystem::path meta = MetaPath(path);

		CompiledModule cached;
		if (ReadMeta(meta, cached))
		{
			uint64_t hash = 0;
			if (ComputeKey(cached.Dependencies, hash))
			{
				bool complete = true;
				for (auto& entry : cached.EntryPoints)
					complete = complete && ReadSpirv(SpirvPath(path, entry.Name, hash), entry.SpirV);

				if (complete)
					return cached;
			}
		}

		TF_CORE_INFO("Shader cache miss: {0}", path.filename().string());

		auto compiled = ShaderCompiler::Compile(path);
		if (!compiled)
			return compiled;

		uint64_t hash = 0;
		if (ComputeKey(compiled->Dependencies, hash))
		{
			for (const auto& entry : compiled->EntryPoints)
			{
				std::ofstream out(SpirvPath(path, entry.Name, hash), std::ios::binary | std::ios::trunc);
				out.write((const char*)entry.SpirV.data(), (std::streamsize)(entry.SpirV.size() * sizeof(uint32_t)));
			}

			WriteMeta(meta, *compiled);
		}

		return compiled;
	}
}
