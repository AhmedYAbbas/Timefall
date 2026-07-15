#pragma once

#include "Timefall/Core/Core.h"

#include <cstdint>

namespace Timefall
{
	enum class GPUMemCategory : uint8_t { Textures = 0, Buffers, Framebuffers, Count };

	// Byte ledger of GL allocations keyed by (category, GL object id). Re-Track with the same
	// id replaces the old size (resize). "Tracked VRAM" = bytes requested; driver padding unseen.
	class TF_API GPUMemoryTracker
	{
	public:
		static void Track(GPUMemCategory category, uint32_t id, uint64_t bytes);
		static void Untrack(GPUMemCategory category, uint32_t id);

		static uint64_t GetBytes(GPUMemCategory category);
		static uint64_t GetTotalBytes();
	};
}
