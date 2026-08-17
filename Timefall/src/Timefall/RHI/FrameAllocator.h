#pragma once

#include "Timefall/RHI/GpuBuffer.h"

namespace Timefall::RHI
{
	struct FrameAllocation
	{
		GpuBuffer* Buffer = nullptr;
		uint64_t Offset = 0;
		void* Mapped = nullptr;

		bool IsValid() const { return Buffer != nullptr; }
	};

	class TF_API FrameAllocator
	{
	public:
		static void Init(uint64_t bytesPerSlot = 8 * 1024 * 1024);
		static void Shutdown();

		static void BeginFrame(uint32_t slot);

		static FrameAllocation Allocate(uint64_t size, uint64_t alignment = 16);

		static uint64_t GetBytesUsed();
		static uint64_t GetCapacity();
	};
}
