#include "tfpch.h"

#include "Timefall/Renderer/GPUProfiler.h"

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/RenderDevice.h"

#include "Platform/Vulkan/VulkanContext.h"

#ifdef TRACY_ENABLE
#include <tracy/TracyVulkan.hpp>
#endif

namespace Timefall
{
#ifdef TRACY_ENABLE
	namespace
	{
		TracyVkCtx s_Context = nullptr;
		vk::CommandPool s_Pool;
		vk::CommandBuffer s_Cmd;

		constexpr size_t MAX_ZONE_DEPTH = 16;
		alignas(tracy::VkCtxScope) std::byte s_ZoneStorage[MAX_ZONE_DEPTH][sizeof(tracy::VkCtxScope)];
		size_t s_ZoneDepth = 0;

		VkCommandBuffer CurrentCommandBuffer()
		{
			RHI::CommandList* list = RHI::RenderDevice::Get().GetCurrentCommandList();
			return list ? (VkCommandBuffer)list->GetNativeHandle() : VK_NULL_HANDLE;
		}
	}

	void GPUProfiler::Init()
	{
		auto& ctx = VulkanContext::Get();
		auto device = ctx.GetDevice();

		// Tracy's calibration sequence begins/ends this buffer several times in a row without
		// resetting the pool between calls, so it needs individual-buffer reset, not just Transient.
		auto pool = device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				.queueFamilyIndex = ctx.GetGraphicsQueueFamily()});
		if (!pool)
		{
			TF_CORE_ERROR("GPUProfiler: createCommandPool failed: {0}", vk::to_string(pool.error()));
			return;
		}

		s_Pool = *pool;

		auto buffers =
			device.allocateCommandBuffers({.commandPool = s_Pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1});
		if (!buffers)
		{
			TF_CORE_ERROR("GPUProfiler: allocateCommandBuffers failed: {0}", vk::to_string(buffers.error()));
			return;
		}

		s_Cmd = (*buffers)[0];

		s_Context = TracyVkContext(ctx.GetInstance(), ctx.GetPhysicalDevice(), device, ctx.GetGraphicsQueue(), s_Cmd,
			VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr, VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
		ctx.SetObjectName(s_Pool, "TracyGPU:Pool");
		ctx.SetObjectName(s_Cmd, "TracyGPU:Cmd");
		TF_CORE_INFO("Tracy GPU profiling initialised");
	}

	void GPUProfiler::Shutdown()
	{
		if (!s_Context)
			return;

		TracyVkDestroy(s_Context);
		s_Context = nullptr;

		VulkanContext::Get().GetDevice().destroyCommandPool(s_Pool);
		s_Pool = nullptr;
	}

	void GPUProfiler::Collect()
	{
		const VkCommandBuffer cmd = CurrentCommandBuffer();
		if (!s_Context || !cmd)
			return;

		TracyVkCollect(s_Context, cmd);
	}

	void GPUProfiler::BeginZone(const char* name)
	{
		const VkCommandBuffer cmd = CurrentCommandBuffer();
		if (!s_Context || !cmd || s_ZoneDepth >= MAX_ZONE_DEPTH)
			return;

		new (&s_ZoneStorage[s_ZoneDepth]) tracy::VkCtxScope(
			s_Context, TracyLine, TracyFile, strlen(TracyFile), TracyFunction, strlen(TracyFunction), name, strlen(name), cmd, true);
		++s_ZoneDepth;
	}

	void GPUProfiler::EndZone()
	{
		if (s_ZoneDepth == 0)
			return;

		--s_ZoneDepth;
		std::launder(reinterpret_cast<tracy::VkCtxScope*>(&s_ZoneStorage[s_ZoneDepth]))->~VkCtxScope();
	}
#else
	void GPUProfiler::Init() {}
	void GPUProfiler::Shutdown() {}
	void GPUProfiler::Collect() {}
	void GPUProfiler::BeginZone(const char*) {}
	void GPUProfiler::EndZone() {}
#endif
}
