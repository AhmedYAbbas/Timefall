#include "tfpch.h"

#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/RenderDevice.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanRHIImpl.h"
#include "Platform/Vulkan/VulkanUploadContext.h"
#include "Platform/Vulkan/GPUMemoryTracker.h"

#include <atomic>

namespace Timefall::RHI
{
	static std::atomic<uint32_t> s_NextTrackerID{1};

	static vk::BufferUsageFlags ToVkUsage(BufferUsage usage)
	{
		vk::BufferUsageFlags flags;
		if (HasFlag(usage, BufferUsage::Vertex))
			flags |= vk::BufferUsageFlagBits::eVertexBuffer;
		if (HasFlag(usage, BufferUsage::Index))
			flags |= vk::BufferUsageFlagBits::eIndexBuffer;
		if (HasFlag(usage, BufferUsage::Uniform))
			flags |= vk::BufferUsageFlagBits::eUniformBuffer;
		if (HasFlag(usage, BufferUsage::Storage))
			flags |= vk::BufferUsageFlagBits::eStorageBuffer;
		if (HasFlag(usage, BufferUsage::TransferSrc))
			flags |= vk::BufferUsageFlagBits::eTransferSrc;
		if (HasFlag(usage, BufferUsage::TransferDst))
			flags |= vk::BufferUsageFlagBits::eTransferDst;

		return flags;
	}

	Ref<GpuBuffer> GpuBuffer::Create(const GpuBufferDesc& desc)
	{
		TF_PROFILE_FUNCTION();
		TF_CORE_ASSERT(desc.Size > 0, "GpuBuffer needs a non-zero size");

		Ref<GpuBuffer> buffer(new GpuBuffer());
		buffer->m_Impl = new Impl();
		Impl& impl = *buffer->m_Impl;
		impl.Size = desc.Size;

		const vk::BufferCreateInfo bufferInfo{.size = desc.Size, .usage = ToVkUsage(desc.Usage), .sharingMode = vk::SharingMode::eExclusive};

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		if (desc.Memory == MemoryType::HostWrite)
		{
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		}

		VkBuffer raw = VK_NULL_HANDLE;
		VmaAllocationInfo allocated{};
		const VkResult result = vmaCreateBuffer(VulkanContext::Get().GetAllocator(), reinterpret_cast<const VkBufferCreateInfo*>(&bufferInfo), &allocInfo, &raw, &impl.Allocation, &allocated);
		if (result != VK_SUCCESS)
		{
			TF_CORE_ERROR("vmaCreateBuffer failed for '{0}' ({1} bytes): {2}", desc.DebugName ? desc.DebugName : "<unnamed>", desc.Size,
				vk::to_string((vk::Result)result));
			return buffer;
		}

		impl.Buffer = raw;
		impl.Mapped = allocated.pMappedData;

		if (desc.DebugName)
			VulkanContext::Get().SetObjectName(impl.Buffer, desc.DebugName);

		impl.TrackerId = s_NextTrackerID++;
		GPUMemoryTracker::Track(GPUMemCategory::Buffers, impl.TrackerId, allocated.size);

		return buffer;
	}

	Ref<GpuBuffer> GpuBuffer::CreateWithData(const GpuBufferDesc& desc, const void* data)
	{
		TF_PROFILE_FUNCTION();
		TF_CORE_ASSERT(desc.Memory == MemoryType::DeviceLocal, "CreateWithData is the DeviceLocal path; HostWrite uses Write");

		GpuBufferDesc target = desc;
		target.Usage = target.Usage | BufferUsage::TransferDst;

		Ref<GpuBuffer> buffer = Create(target);
		if (!buffer->IsValid())
			return buffer;

		VulkanUploadContext::UploadBuffer(buffer->m_Impl->Buffer, 0, data, desc.Size);
		return buffer;
	}

	UploadScope::UploadScope()
	{
		VulkanUploadContext::Begin();
	}

	UploadScope::~UploadScope()
	{
		VulkanUploadContext::End();
	}

	GpuBuffer::~GpuBuffer()
	{
		if (!m_Impl)
			return;

		if (m_Impl->Buffer)
		{
			GPUMemoryTracker::Untrack(GPUMemCategory::Buffers, m_Impl->TrackerId);

			RenderDevice::Get().DeferDestroy([buffer = m_Impl->Buffer, allocation = m_Impl->Allocation]()
			{
				vmaDestroyBuffer(VulkanContext::Get().GetAllocator(), (VkBuffer)buffer, allocation);
			});
		}

		delete m_Impl;
		m_Impl = nullptr;
	}

	bool GpuBuffer::Write(const void* data, uint64_t size, uint64_t offset)
	{
		if (!m_Impl || !m_Impl->Mapped)
		{
			TF_CORE_ERROR("GpuBuffer::Write on a buffer with no host mapping - it must be MemoryType::HostWrite");
			return false;
		}

		if (offset + size > m_Impl->Size)
		{
			TF_CORE_ERROR("GpuBuffer::Write of {0} bytes at offset {1} exceeds the buffer's {2}", size, offset, m_Impl->Size);
			return false;
		}

		std::memcpy((uint8_t*)m_Impl->Mapped + offset, data, size);
		vmaFlushAllocation(VulkanContext::Get().GetAllocator(), m_Impl->Allocation, offset, size);
		return true;
	}

	void* GpuBuffer::GetMapped() const
	{
		return m_Impl ? m_Impl->Mapped : nullptr;
	}

	uint64_t GpuBuffer::Size() const
	{
		return m_Impl ? m_Impl->Size : 0;
	}

	bool GpuBuffer::IsValid() const
	{
		return m_Impl && m_Impl->Buffer;
	}
}
