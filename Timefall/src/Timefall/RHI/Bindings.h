#pragma once

#include "Timefall/Core/Core.h"

#include <cstdint>

namespace Timefall::RHI
{
	enum class SamplerSlot : uint32_t {
		LinearRepeat = 0, // every material texture: linear, mip-linear, repeat, max anisotropy
		LinearClampEdge, // MSDF atlases, IBL, fullscreen resolves
		NearestClampEdge, // point-sampled reads: entity IDs, unfiltered blits
		Count
	};

	inline constexpr uint64_t FrameUniformSlotBytes = 256;
	inline constexpr uint64_t PassUniformSlotBytes = 4608;
	inline constexpr uint32_t PassUniformSlices = 16; // per frame in flight

	class TF_API Bindings
	{
	public:
		static bool WriteFrameUniforms(const void* data, uint64_t size);
		static uint32_t WritePassUniforms(const void* data, uint64_t size);

		static uint32_t GetBindlessCapacity();
		static uint32_t GetBindlessUsed();

		static uint32_t GetWhiteTextureIndex();
	};
}
