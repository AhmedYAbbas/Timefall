#pragma once

#include <cstdint>
#include <string_view>

namespace Timefall
{
	inline constexpr uint64_t FNV_OFFSET = 14695981039346656037ull;
	inline constexpr uint64_t FNV_PRIME = 1099511628211ull;

	inline void HashBytes(uint64_t& hash, const void* data, size_t size)
	{
		const auto* bytes = (const uint8_t*)data;
		for (size_t i = 0; i < size; i++)
		{
			hash ^= bytes[i];
			hash *= FNV_PRIME;
		}
	}

	inline void HashString(uint64_t& hash, std::string_view text)
	{
		HashBytes(hash, text.data(), text.size());
	}
}
