#pragma once

#include "Timefall/RHI/GpuBuffer.h"

namespace Timefall::RHI
{
	// A slice of the current frame slot's streaming buffer. Borrowed, not owned: valid only until
	// this slot comes round again, which is exactly one frame's worth of GPU work away.
	struct FrameAllocation
	{
		GpuBuffer* Buffer = nullptr;
		uint64_t Offset = 0;
		void* Mapped = nullptr;

		bool IsValid() const { return Buffer != nullptr; }
	};

	// One HostWrite buffer per frame-in-flight slot, bump-allocated. The single source of per-frame
	// streaming memory: vertex streams, dynamic uniform slices, per-draw storage.
	class TF_API FrameAllocator
	{
	public:
		static void Init(uint64_t bytesPerSlot = 8 * 1024 * 1024);
		static void Shutdown();

		// Called by RenderDevice::BeginFrame once WaitForSlot has proven this slot retired.
		static void BeginFrame(uint32_t slot);

		// `alignment` must be a power of two, and for uniform/storage slices must come from
		// RHI::Limits - the 16-byte default satisfies vertex and index buffers only. Returns an
		// invalid allocation when the slot is full; callers drop the draw rather than treat it as fatal.
		static FrameAllocation Allocate(uint64_t size, uint64_t alignment = 16);

		static uint64_t GetBytesUsed();
		static uint64_t GetCapacity();
	};
}
