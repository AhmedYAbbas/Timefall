#include "tfpch.h"
#include "FrameAllocator.h"

namespace Timefall::RHI
{
	namespace
	{
		std::array<Ref<GpuBuffer>, FRAMES_IN_FLIGHT> s_Slots;
		uint64_t s_Capacity = 0;
		uint64_t s_PendingCapacity = 0; // applied at the next BeginFrame; never mid-frame
		uint64_t s_Offset = 0;
		uint64_t s_Slot = 0;
		bool s_Overflowed = false;

		void AllocateSlots(uint64_t bytesPerSlot)
		{
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
			{
				const std::string name = std::format("FrameAllocator[{}]", i);
				s_Slots[i] = GpuBuffer::Create({.Size = bytesPerSlot,
					.Usage =
						BufferUsage::Vertex | BufferUsage::Index | BufferUsage::Uniform | BufferUsage::Storage | BufferUsage::TransferSrc | BufferUsage::ShaderDeviceAddress,
					.Memory = MemoryType::HostWrite,
					.DebugName = name.c_str()});
			}

			s_Capacity = bytesPerSlot;
		}
	}

	void FrameAllocator::Init(uint64_t bytesPerSlot)
	{
		AllocateSlots(bytesPerSlot);
		s_PendingCapacity = bytesPerSlot;
		s_Offset = 0;
		s_Slot = 0;
		s_Overflowed = false;

		TF_CORE_INFO("FrameAllocator: {0} KB x {1} slots, slot 0 device address 0x{2:x}", bytesPerSlot / 1024, (uint32_t)FRAMES_IN_FLIGHT,
			s_Slots[0] ? s_Slots[0]->GetDeviceAddress() : 0);
	}

	void FrameAllocator::Shutdown()
	{
		for (auto& slot : s_Slots)
			slot.reset();

		s_Capacity = 0;
		s_PendingCapacity = 0;
		s_Offset = 0;
	}

	void FrameAllocator::BeginFrame(uint32_t slot)
	{
		// Growth happens here and only here: replacing a slot buffer mid-frame would destroy one
		// already bound into the recording command buffer.
		if (s_PendingCapacity > s_Capacity)
		{
			TF_CORE_WARN("FrameAllocator growing {0} KB -> {1} KB per slot", s_Capacity / 1024, s_PendingCapacity / 1024);
			AllocateSlots(s_PendingCapacity); // old Refs drop into the deletion queue
			s_Overflowed = false;
		}

		s_Slot = slot % FRAMES_IN_FLIGHT;
		s_Offset = 0;
	}

	FrameAllocation FrameAllocator::Allocate(uint64_t size, uint64_t alignment)
	{
		const Ref<GpuBuffer>& buffer = s_Slots[s_Slot];
		if (!buffer || !buffer->IsValid() || size == 0)
			return {};

		const uint64_t aligned = (s_Offset + alignment - 1) & ~(alignment - 1);
		if (aligned + size > s_Capacity)
		{
			// Once per growth cycle: a full ring would otherwise log every draw of every frame.
			if (!s_Overflowed)
			{
				TF_CORE_ERROR("FrameAllocator slot exhausted ({0} KB); growing next frame", s_Capacity / 1024);
				s_Overflowed = true;
			}

			s_PendingCapacity = std::max(s_PendingCapacity, (aligned + size) * 2);
			return {};
		}

		s_Offset = aligned + size;
		return {buffer.get(), aligned, (uint8_t*)buffer->GetMapped() + aligned};
	}

	uint64_t FrameAllocator::GetBytesUsed()
	{
		return s_Offset;
	}

	uint64_t FrameAllocator::GetCapacity()
	{
		return s_Capacity;
	}
}
