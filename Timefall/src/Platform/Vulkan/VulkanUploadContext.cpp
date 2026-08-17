#include "tfpch.h"
#include "VulkanUploadContext.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/GPUMemoryTracker.h"

namespace Timefall
{
	namespace
	{
		vk::CommandPool s_Pool;
		vk::CommandBuffer s_Cmd;
		vk::Semaphore s_Timeline;
		uint64_t s_Value = 0;

		vk::Buffer s_Staging;
		VmaAllocation s_StagingAlloc = nullptr;
		uint8_t* s_StagingMapped = nullptr;
		uint64_t s_StagingCapacity = 0;
		uint64_t s_StagingOffset = 0;
		uint64_t s_CopyAlignment = 16;

		uint32_t s_ScopeDepth = 0;
		bool s_Recording = false;

		// VulkanBuffer.cpp mints tracker ids from 1 upward, so the top of the range is permanently
		// free for this one backend-private allocation.
		constexpr uint32_t STAGING_TRACKER_ID = UINT32_MAX;

		constexpr uint64_t Align(uint64_t value, uint64_t alignment)
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		void EnsureRecording()
		{
			if (s_Recording)
				return;

			(void)VulkanContext::Get().GetDevice().resetCommandPool(s_Pool);
			(void)s_Cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			s_Recording = true;
		}

		void FlushIfUnscoped()
		{
			if (s_ScopeDepth == 0)
				VulkanUploadContext::Flush();
		}
	}

	void VulkanUploadContext::Init(uint64_t stagingBytes)
	{
		TF_CORE_ASSERT(stagingBytes > 0, "Upload context needs a non-zero staging ring");

		auto device = VulkanContext::Get().GetDevice();

		auto pool = device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = VulkanContext::Get().GetGraphicsQueueFamily()});
		if (!pool)
		{
			TF_CORE_ERROR("Upload context createCommandPool failed: {0}", vk::to_string(pool.error()));
			return;
		}

		s_Pool = *pool;

		auto buffers =
			device.allocateCommandBuffers({.commandPool = s_Pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});
		if (!buffers)
		{
			TF_CORE_ERROR("Upload context allocateCommandBuffers failed: {0}", vk::to_string(buffers.error()));
			return;
		}

		s_Cmd = (*buffers)[0];

		// Its own timeline, never the frame ring's: that counter is the frame clock, and
		// `waitValue = signalValue - FRAMES_IN_FLIGHT` would read a foreign signal as a retired frame.
		// StructureChain, matching VulkanFrameRing::Init - a raw pNext to a local would dangle.
		const vk::StructureChain timelineInfo{
			vk::SemaphoreCreateInfo{}, vk::SemaphoreTypeCreateInfo{.semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0}};

		auto semaphore = device.createSemaphore(timelineInfo.get<vk::SemaphoreCreateInfo>());
		if (!semaphore)
		{
			TF_CORE_ERROR("Upload context createSemaphore failed: {0}", vk::to_string(semaphore.error()));
			return;
		}

		s_Timeline = *semaphore;
		s_Value = 0;

		VulkanContext::Get().SetObjectName(s_Timeline, "UploadTimeline");

		const vk::BufferCreateInfo stagingInfo{
			.size = stagingBytes, .usage = vk::BufferUsageFlagBits::eTransferSrc, .sharingMode = vk::SharingMode::eExclusive};

		// AUTO_PREFER_HOST, not the AUTO that GpuBuffer::Create uses: the GPU only ever reads this
		// over DMA, so parking it in the BAR window would spend scarce device-visible memory for
		// nothing. That knob is why the ring is allocated by hand rather than through GpuBuffer.
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		VkBuffer raw = VK_NULL_HANDLE;
		VmaAllocationInfo allocated{};
		const VkResult result = vmaCreateBuffer(VulkanContext::Get().GetAllocator(),
			reinterpret_cast<const VkBufferCreateInfo*>(&stagingInfo), &allocInfo, &raw, &s_StagingAlloc, &allocated);
		if (result != VK_SUCCESS)
		{
			TF_CORE_ERROR("Upload context staging ring ({0} KB) failed: {1}", stagingBytes / 1024, vk::to_string((vk::Result)result));
			return;
		}

		s_Staging = raw;
		s_StagingMapped = (uint8_t*)allocated.pMappedData;
		s_StagingCapacity = stagingBytes;
		s_StagingOffset = 0;

		VulkanContext::Get().SetObjectName(s_Staging, "UploadStagingRing");
		GPUMemoryTracker::Track(GPUMemCategory::Buffers, STAGING_TRACKER_ID, allocated.size);

		s_CopyAlignment = std::max<uint64_t>(
			VulkanContext::Get().GetPhysicalDevice().getProperties2().properties.limits.optimalBufferCopyOffsetAlignment, 16);

		TF_CORE_INFO("UploadContext: {0} KB staging ring", stagingBytes / 1024);
	}

	void VulkanUploadContext::Shutdown()
	{
		TF_CORE_ASSERT(s_ScopeDepth == 0, "An UploadScope is still open at shutdown");

		Flush();

		auto device = VulkanContext::Get().GetDevice();

		if (s_Staging)
		{
			GPUMemoryTracker::Untrack(GPUMemCategory::Buffers, STAGING_TRACKER_ID);
			vmaDestroyBuffer(VulkanContext::Get().GetAllocator(), (VkBuffer)s_Staging, s_StagingAlloc);
		}

		if (s_Timeline)
			device.destroySemaphore(s_Timeline);
		if (s_Pool)
			device.destroyCommandPool(s_Pool); // frees s_Cmd with it

		s_Staging = nullptr;
		s_StagingAlloc = nullptr;
		s_StagingMapped = nullptr;
		s_StagingCapacity = 0;
		s_StagingOffset = 0;
		s_Timeline = nullptr;
		s_Pool = nullptr;
		s_Cmd = nullptr;
		s_Value = 0;
	}

	void VulkanUploadContext::Begin()
	{
		s_ScopeDepth++;
	}

	void VulkanUploadContext::End()
	{
		TF_CORE_ASSERT(s_ScopeDepth > 0, "UploadScope closed more times than it was opened");

		if (--s_ScopeDepth == 0)
			Flush();
	}

	bool VulkanUploadContext::UploadBuffer(vk::Buffer dst, uint64_t dstOffset, const void* data, uint64_t size)
	{
		TF_PROFILE_FUNCTION();

		if (!s_Cmd || !s_StagingMapped)
		{
			TF_CORE_ERROR("VulkanUploadContext::UploadBuffer before Init");
			return false;
		}

		const uint8_t* source = (const uint8_t*)data;
		uint64_t written = 0;

		while (written < size)
		{
			uint64_t offset = Align(s_StagingOffset, s_CopyAlignment);
			const uint64_t remaining = size - written;

			// Flush when the tail cannot take the rest in one piece - unless the ring is already
			// empty, in which case the payload is simply bigger than the ring and gets chunked.
			if (offset + remaining > s_StagingCapacity && offset != 0)
			{
				Flush(); // the wait is what proves the ring is free to overwrite
				offset = 0;
			}

			const uint64_t chunk = std::min(remaining, s_StagingCapacity - offset);

			EnsureRecording();
			std::memcpy(s_StagingMapped + offset, source + written, chunk);

			const vk::BufferCopy2 region{.srcOffset = offset, .dstOffset = dstOffset + written, .size = chunk};
			s_Cmd.copyBuffer2({.srcBuffer = s_Staging, .dstBuffer = dst, .regionCount = 1, .pRegions = &region});

			s_StagingOffset = offset + chunk;
			written += chunk;
		}

		FlushIfUnscoped();
		return true;
	}

	bool VulkanUploadContext::UploadImage(
		vk::Image dst, uint32_t width, uint32_t height, uint32_t bytesPerPixel, const void* data, uint64_t size)
	{
		TF_PROFILE_FUNCTION();

		if (!s_Cmd || !s_StagingMapped)
		{
			TF_CORE_ERROR("VulkanUploadContext::UploadImage before Init");
			return false;
		}

		const uint64_t rowBytes = (uint64_t)width * bytesPerPixel;
		if (rowBytes || rowBytes > s_StagingCapacity)
		{
			TF_CORE_ERROR("UploadImage row of {0} bytes does not fit the {1} byte staging ring", rowBytes, s_StagingCapacity);
			return false;
		}

		if (size < rowBytes * height)
		{
			TF_CORE_ERROR("UploadImage got {0} bytes for a {1}x{2} image needing {3}", size, width, height, rowBytes * height);
			return false;
		}

		EnsureRecording();

		const vk::ImageMemoryBarrier2 toDst{.srcStageMask = vk::PipelineStageFlagBits2::eNone,
		.srcAccessMask = vk::AccessFlagBits2::eNone,
		.dstStageMask = vk::PipelineStageFlagBits2::eCopy,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.oldLayout = vk::ImageLayout::eUndefined,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		.image = dst,
		.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		s_Cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toDst});

		const uint8_t* source = (const uint8_t*)data;
		uint32_t row = 0;

		while (row < height)
		{
			uint64_t offset = Align(s_StagingOffset, s_CopyAlignment);
			if (offset + rowBytes > s_StagingCapacity && offset != 0)
			{
				Flush();
				EnsureRecording();

				offset = 0;
			}

			const uint32_t rowsThisChunk = (uint32_t)std::min<uint64_t>(height - row, (s_StagingCapacity - offset) / rowBytes);
			const uint64_t chunkBytes = rowBytes * rowsThisChunk;

			std::memcpy(s_StagingMapped + offset, source + (uint64_t)row * rowBytes, chunkBytes);

			const vk::BufferImageCopy2 region{.bufferOffset = offset,
			.bufferRowLength = width,
			.bufferImageHeight = rowsThisChunk, .imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			.imageOffset = {0, (int32_t)row, 0},
			.imageExtent = {width, rowsThisChunk, 1}};

			s_Cmd.copyBufferToImage2({.srcBuffer = s_Staging,
				.dstImage = dst,
				.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
				.regionCount = 1,
				.pRegions = &region});

			s_StagingOffset = offset + chunkBytes;
			row += rowsThisChunk;
		}

		FlushIfUnscoped();
		return true;
	}

	void VulkanUploadContext::Record(const std::function<void(vk::CommandBuffer)>& fn)
	{
		TF_PROFILE_FUNCTION();

		if (!s_Cmd)
		{
			TF_CORE_ERROR("VulkanUploadContext::Record before Init");
			return;
		}

		EnsureRecording();
		fn(s_Cmd);
		FlushIfUnscoped();
	}

	void VulkanUploadContext::Flush()
	{
		TF_PROFILE_FUNCTION();

		if (!s_Recording)
			return;

		// One global barrier covers every copy in this command buffer, whatever it touched - cheaper
		// to reason about than a BufferMemoryBarrier2 per destination, and this path blocks anyway.
		// Without it, submission order alone does not make the writes visible.
		const vk::MemoryBarrier2 barrier{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
			.dstAccessMask = vk::AccessFlagBits2::eMemoryRead};

		s_Cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &barrier});

		(void)s_Cmd.end();
		s_Recording = false;

		const uint64_t signalValue = ++s_Value;

		const vk::CommandBufferSubmitInfo cmdInfo{.commandBuffer = s_Cmd};
		const vk::SemaphoreSubmitInfo signal{
			.semaphore = s_Timeline, .value = signalValue, .stageMask = vk::PipelineStageFlagBits2::eAllCommands};

		// submit2 and waitSemaphores carry multiple success codes, so vulkan.hpp hands back a bare
		// vk::Result rather than std::expected - discarded here exactly as VulkanFrameRing and
		// RenderDevice::EndFrame already do. Failures surface through the validation layers.
		(void)VulkanContext::Get().GetGraphicsQueue().submit2({vk::SubmitInfo2{.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdInfo,
			.signalSemaphoreInfoCount = 1,
			.pSignalSemaphoreInfos = &signal}});
		(void)VulkanContext::Get().GetDevice().waitSemaphores(
			{.semaphoreCount = 1, .pSemaphores = &s_Timeline, .pValues = &signalValue}, UINT64_MAX);

		s_StagingOffset = 0;
	}
}
