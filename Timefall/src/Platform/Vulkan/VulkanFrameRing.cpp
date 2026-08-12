#include "tfpch.h"
#include "VulkanFrameRing.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Timefall
{
	void VulkanFrameRing::Init(uint32_t swapchainImageCount)
	{
		auto& ctx = VulkanContext::Get();
		auto device = ctx.GetDevice();

		for (uint32_t i = 0; i < RHI::FRAMES_IN_FLIGHT; i++)
		{
			auto& frame = m_Frames[i];

			auto pool = device.createCommandPool(
				{.flags = vk::CommandPoolCreateFlagBits::eTransient, .queueFamilyIndex = ctx.GetGraphicsQueueFamily()});

			if (!pool)
			{
				TF_CORE_ERROR("createCommandPool failed: {0}", vk::to_string(pool.error()));
				return;
			}

			frame.Pool = *pool;

			auto buffers = device.allocateCommandBuffers(
				{.commandPool = frame.Pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});

			if (!buffers)
			{
				TF_CORE_ERROR("allocateCommandBuffers failed: {0}", vk::to_string(buffers.error()));
				return;
			}

			frame.Cmd = (*buffers)[0];

			auto acquired = device.createSemaphore({});
			if (!acquired)
			{
				TF_CORE_ERROR("createSemaphore failed: {0}", vk::to_string(acquired.error()));
				return;
			}

			frame.ImageAvailable = *acquired;

			ctx.SetObjectName(frame.Pool, std::format("FrameCommandPool[{}]", i).c_str());
			ctx.SetObjectName(frame.Cmd, std::format("FrameCmd[{}]", i).c_str());
			ctx.SetObjectName(frame.ImageAvailable, std::format("ImageAvailable[{}]", i).c_str());
		}

		if (!CreateRenderFinishedSemaphores(swapchainImageCount))
			return;

		const vk::StructureChain timelineInfo{
			vk::SemaphoreCreateInfo{},
			vk::SemaphoreTypeCreateInfo{.semaphoreType = vk::SemaphoreType::eTimeline, .initialValue = 0},
		};

		auto timeline = device.createSemaphore(timelineInfo.get<vk::SemaphoreCreateInfo>());
		if (!timeline)
		{
			TF_CORE_ERROR("createSemaphore (timeline) failed: {0}", vk::to_string(timeline.error()));
			return;
		}

		m_Timeline = *timeline;

		ctx.SetObjectName(m_Timeline, "FrameTimeline");
	}

	bool VulkanFrameRing::CreateRenderFinishedSemaphores(uint32_t swapchainImageCount)
	{
		auto& ctx = VulkanContext::Get();
		auto device = ctx.GetDevice();

		m_RenderFinished.resize(swapchainImageCount);
		for (uint32_t i = 0; i < swapchainImageCount; i++)
		{
			auto sem = device.createSemaphore({});
			if (!sem)
			{
				TF_CORE_ERROR("createSemaphore (renderFinished) failed: {0}", vk::to_string(sem.error()));
				return false;
			}

			m_RenderFinished[i] = *sem;
			ctx.SetObjectName(m_RenderFinished[i], std::format("RenderFinished[{}]", i).c_str());
		}

		return true;
	}

	void VulkanFrameRing::OnSwapchainResized(uint32_t swapchainImageCount)
	{
		if (m_RenderFinished.size() == swapchainImageCount)
			return;

		// Safe to destroy: every caller rebuilds the swapchain behind a waitIdle, so no
		// present is still pending on these.
		auto device = VulkanContext::Get().GetDevice();
		for (auto s : m_RenderFinished)
			device.destroySemaphore(s);

		m_RenderFinished.clear();
		(void)CreateRenderFinishedSemaphores(swapchainImageCount);
	}

	uint64_t VulkanFrameRing::WaitForSlot()
	{
		auto device = VulkanContext::Get().GetDevice();

		const uint64_t signalValue = ++m_FrameValue;
		const uint64_t waitValue = signalValue >= RHI::FRAMES_IN_FLIGHT ? signalValue - RHI::FRAMES_IN_FLIGHT : 0;

		(void)device.waitSemaphores({.semaphoreCount = 1, .pSemaphores = &m_Timeline, .pValues = &waitValue}, UINT64_MAX);

		return signalValue;
	}

	uint64_t VulkanFrameRing::CompletedValue() const
	{
		auto value = VulkanContext::Get().GetDevice().getSemaphoreCounterValue(m_Timeline);
		return value ? *value : 0;
	}

	void VulkanFrameRing::Shutdown()
	{
		auto device = VulkanContext::Get().GetDevice();

		for (auto& frame : m_Frames)
		{
			device.destroySemaphore(frame.ImageAvailable);
			device.destroyCommandPool(frame.Pool);
		}

		for (auto s : m_RenderFinished)
			device.destroySemaphore(s);

		m_RenderFinished.clear();

		device.destroySemaphore(m_Timeline);
		m_Timeline = nullptr;
	}
}
