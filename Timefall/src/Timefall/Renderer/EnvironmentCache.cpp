#include "tfpch.h"
#include "EnvironmentCache.h"

#include "Timefall/Core/Hash.h"

#include <cstring>

namespace Timefall
{
	static constexpr char MAGIC[5] = {'T', 'F', 'E', 'N', 'V'};
	static constexpr uint32_t FORMAT_VERSION = 1;

	// The bake shaders are part of the key: editing a sample count or the convolution step has to invalidate every file
	static const char* BAKE_SHADERS[2] = {"Assets/Shaders/Renderer3D_IrradianceConvolve.slang", "Assets/Shaders/Renderer3D_Prefilter.slang"};

	static bool ReadBinaryFile(const std::filesystem::path& path, std::vector<std::byte>& out)
	{
		std::ifstream in(path, std::ios::in | std::ios::binary | std::ios::ate);
		if (!in)
			return false;

		const std::streamsize size = in.tellg();
		if (size < 0)
			return false;

		out.resize((size_t)size);
		in.seekg(0, std::ios::beg);
		in.read((char*)out.data(), size);

		return (bool)in;
	}

	static std::filesystem::path EntryPath(const std::filesystem::path& hdrPath, uint64_t key)
	{
		return EnvironmentCache::Directory() / std::format("{}.{:016x}.tfenv", hdrPath.stem().string(), key);
	}

	const std::filesystem::path& EnvironmentCache::Directory()
	{
		static const std::filesystem::path directory = "Cache/Environments";
		return directory;
	}

	std::optional<uint64_t> EnvironmentCache::ComputeKey(const std::filesystem::path& hdrPath, const EnvBakeParams& params)
	{
		TF_PROFILE_FUNCTION();

		if (hdrPath.empty())
			return std::nullopt;

		std::vector<std::byte> contents;
		if (!ReadBinaryFile(hdrPath, contents))
		{
			TF_CORE_INFO("Environment cache: cannot read '{0}' for keying - the bake will not be cached", hdrPath.string());
			return std::nullopt;
		}

		uint64_t hash = FNV_OFFSET;
		HashBytes(hash, contents.data(), contents.size());

		for (const char* shader : BAKE_SHADERS)
		{
			std::vector<std::byte> source;
			if (!ReadBinaryFile(shader, source))
			{
				TF_CORE_INFO("Environment cache: cannot read '{0}' for keying - the bake will not be cached", shader);
				return std::nullopt;
			}

			HashBytes(hash, source.data(), source.size());
		}

		HashBytes(hash, &params, sizeof(params));
		HashBytes(hash, &FORMAT_VERSION, sizeof(FORMAT_VERSION));

		return hash;
	}

	std::optional<EnvBlobs> EnvironmentCache::TryLoad(const std::filesystem::path& hdrPath, const EnvBakeParams& params, uint64_t key)
	{
		TF_PROFILE_FUNCTION();

		const std::filesystem::path path = EntryPath(hdrPath, key);

		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in)
		{
			TF_CORE_INFO("Environment cache miss: no '{0}'", path.string());
			return std::nullopt;
		}

		char magic[5]{};
		uint32_t version = 0;
		uint64_t storedKey = 0;
		EnvBakeParams stored{};

		in.read(magic, sizeof(magic));
		in.read((char*)&version, sizeof(version));
		in.read((char*)&storedKey, sizeof(storedKey));
		in.read((char*)&stored, sizeof(stored));

		if (!in || std::memcmp(magic, MAGIC, sizeof(MAGIC)) != 0 || version != FORMAT_VERSION || storedKey != key
			|| std::memcmp(&stored, &params, sizeof(params)) != 0)
		{
			TF_CORE_INFO("Environment cache miss: '{0}' is not a matching TFENV entry", path.string());
			return std::nullopt;
		}

		EnvBlobs blobs;
		blobs.Irradiance.resize(IrradianceBytes(params));
		blobs.Prefilter.resize(PrefilterBytes(params));

		in.read((char*)blobs.Irradiance.data(), (std::streamsize)blobs.Irradiance.size());
		in.read((char*)blobs.Prefilter.data(), (std::streamsize)blobs.Prefilter.size());

		if (!in)
		{
			TF_CORE_INFO("Environment cache miss: '{0}' is truncated", path.string());
			return std::nullopt;
		}

		TF_CORE_INFO("Environment cache hit: '{0}'", path.string());
		return blobs;
	}

	void EnvironmentCache::Store(const std::filesystem::path& hdrPath, const EnvBakeParams& params, uint64_t key, const EnvBlobs& blobs)
	{
		TF_PROFILE_FUNCTION();

		std::error_code ec;
		std::filesystem::create_directories(Directory(), ec);

		const std::filesystem::path path = EntryPath(hdrPath, key);

		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out)
		{
			TF_CORE_WARN("Environment cache: could not open '{0}' for writing", path.string());
			return;
		}

		out.write(MAGIC, sizeof(MAGIC));
		out.write((const char*)&FORMAT_VERSION, sizeof(FORMAT_VERSION));
		out.write((const char*)&key, sizeof(key));
		out.write((const char*)&params, sizeof(params));
		out.write((const char*)blobs.Irradiance.data(), (std::streamsize)blobs.Irradiance.size());
		out.write((const char*)blobs.Prefilter.data(), (std::streamsize)blobs.Prefilter.size());

		if (!out)
			TF_CORE_WARN("Environment cache: writing '{0}' failed", path.string());
		else
			TF_CORE_INFO("Environment cache stored: '{0}'", path.string());
	}
}
