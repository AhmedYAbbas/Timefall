#pragma once

#include "Timefall/RHI/RHITypes.h"

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <vector>

namespace Timefall
{
	struct FrameData
	{
		vk::CommandPool Pool;
		vk::CommandBuffer Cmd;
		vk::Semaphore ImageAvailable;
	};

	class VulkanFrameRing
	{
	public:
		void Init(uint32_t swapchainImageCount);
		void Shutdown();

		// Must follow every swapchain rebuild: the render-finished semaphores are
		// indexed by swapchain image, so a changed image count invalidates the array.
		void OnSwapchainResized(uint32_t swapchainImageCount);

		uint64_t WaitForSlot();

		void AbandonFrame() { --m_FrameValue; }

		FrameData& Current() { return m_Frames[m_FrameValue % RHI::FRAMES_IN_FLIGHT]; }
		vk::Semaphore Timeline() const { return m_Timeline; }
		uint64_t FrameValue() const { return m_FrameValue; }

		uint64_t CompletedValue() const;

		vk::Semaphore RenderFinished(uint32_t imageIndex) const { return m_RenderFinished[imageIndex]; }

	private:
		bool CreateRenderFinishedSemaphores(uint32_t swapchainImageCount);

		std::array<FrameData, RHI::FRAMES_IN_FLIGHT> m_Frames{};
		std::vector<vk::Semaphore> m_RenderFinished;
		vk::Semaphore m_Timeline;
		uint64_t m_FrameValue = 0;
	};
}
