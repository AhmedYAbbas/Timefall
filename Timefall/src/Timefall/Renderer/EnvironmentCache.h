#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	struct EnvBakeParams
	{
		uint32_t IrrSize = 0;
		uint32_t PreSize = 0;
		uint32_t PreMips = 0;
		uint32_t IrrSamples = 0;
		uint32_t PreSamples = 0;
	};

	struct EnvBlobs
	{
		std::vector<std::byte> Irradiance;
		std::vector<std::byte> Prefilter;
	};

	// RGBA16F
	inline constexpr uint32_t kEnvBytesPerTexel = 8;

	inline constexpr uint64_t IrradianceBytes(const EnvBakeParams& p)
	{
		return (uint64_t)p.IrrSize * p.IrrSize * kEnvBytesPerTexel * 6;
	}

	inline constexpr uint64_t PrefilterBytes(const EnvBakeParams& p)
	{
		uint64_t total = 0;
		for (uint64_t mip = 0; mip < p.PreMips; mip++)
		{
			const uint64_t edge = p.PreSize >> mip ? p.PreSize >> mip : 1;
			total += edge * edge * kEnvBytesPerTexel * 6;
		}

		return total;
	}

	class TF_API EnvironmentCache
	{
	public:
		static const std::filesystem::path& Directory();

		static std::optional<uint64_t> ComputeKey(const std::filesystem::path& hdrPath, const EnvBakeParams& params);

		static std::optional<EnvBlobs> TryLoad(const std::filesystem::path& hdrPath, const EnvBakeParams& params, uint64_t key);

		static void Store(const std::filesystem::path& hdrPath, const EnvBakeParams& params, uint64_t key, const EnvBlobs& blobs);
	};
}
