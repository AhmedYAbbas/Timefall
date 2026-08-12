#include "tfpch.h"

#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/CommandList.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanFrameRing.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"

#include <GLFW/glfw3.h>

namespace Timefall::RHI
{
	struct CommandList::Impl
	{
		vk::CommandBuffer Cmd;
		vk::Image TargetImage;
		vk::ImageView TargetView;
		vk::Extent2D TargetExtent;
		vk::ImageLayout* TargetLayout = nullptr; // points at the owner's layout slot, updated in place
		bool InPass = false;
	};

	struct RenderDevice::Impl
	{
		VulkanSwapchain Swapchain;
		VulkanFrameRing Frames;
		VulkanDeletionQueue DeletionQueue;

		CommandList List;
		CommandList::Impl ListImpl;

		uint32_t ImageIndex = 0;
		uint64_t FrameSignalValue = 0;
		bool FrameActive = false;
		GLFWwindow* Window = nullptr;
	};

	static void TransitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		vk::PipelineStageFlags2 srcStage, vk::AccessFlags2 srcAccess, vk::PipelineStageFlags2 dstStage, vk::AccessFlags2 dstAccess)
	{
		const vk::ImageMemoryBarrier2 barrier{.srcStageMask = srcStage,
			.srcAccessMask = srcAccess,
			.dstStageMask = dstStage,
			.dstAccessMask = dstAccess,
			.oldLayout = oldLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = image,
			.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

		cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
	}

	// --------------------------------CommandList--------------------------------
	void CommandList::BeginPass(const PassDesc& desc)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "BeginPass called while a pass is already open");

		auto& ctx = VulkanContext::Get();
		if (ctx.IsDebugEnabled() && desc.DebugName)
			m_Impl->Cmd.beginDebugUtilsLabelEXT({.pLabelName = desc.DebugName});

		const auto& target = desc.Color[0];

		// Only Load has contents worth preserving; Clear/DontCare take Undefined so the driver
		// can skip the reformat and discard whatever is there.
		const vk::ImageLayout oldLayout = target.Load == LoadOp::Load ? *m_Impl->TargetLayout : vk::ImageLayout::eUndefined;

		// srcStage must match the stage EndFrame waits ImageAvailable on, or the transition
		// (an image write) can run before the presentation engine has released the image.
		// dstAccess needs Read as well as Write: LoadOp::Load and blending both read the attachment.
		TransitionImage(m_Impl->Cmd, m_Impl->TargetImage, oldLayout, vk::ImageLayout::eColorAttachmentOptimal,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead);

		*m_Impl->TargetLayout = vk::ImageLayout::eColorAttachmentOptimal;

		vk::ClearValue clear{};
		clear.color =
			vk::ClearColorValue{std::array{target.ClearValue[0], target.ClearValue[1], target.ClearValue[2], target.ClearValue[3]}};

		const vk::RenderingAttachmentInfo color{.imageView = m_Impl->TargetView,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = target.Load == LoadOp::Clear ? vk::AttachmentLoadOp::eClear
				: target.Load == LoadOp::Load      ? vk::AttachmentLoadOp::eLoad
												   : vk::AttachmentLoadOp::eDontCare,
			.storeOp = target.Store == StoreOp::Store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare,
			.clearValue = clear};

		m_Impl->Cmd.beginRendering(
			{.renderArea = {{0, 0}, m_Impl->TargetExtent}, .layerCount = 1, .colorAttachmentCount = 1, .pColorAttachments = &color});

		m_Impl->InPass = true;

		SetViewport(0, 0, m_Impl->TargetExtent.width, m_Impl->TargetExtent.height);
		SetScissor(0, 0, m_Impl->TargetExtent.width, m_Impl->TargetExtent.height);
	}

	void CommandList::EndPass()
	{
		m_Impl->Cmd.endRendering();
		m_Impl->InPass = false;

		if (VulkanContext::Get().IsDebugEnabled())
			m_Impl->Cmd.endDebugUtilsLabelEXT();
	}

	void CommandList::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		const vk::Viewport vp{
			.x = (float)x, .y = (float)(y + height), .width = (float)width, .height = -(float)height, .minDepth = 0.0f, .maxDepth = 1.0f};

		m_Impl->Cmd.setViewport(0, 1, &vp);
	}

	void CommandList::SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		const vk::Rect2D rect{{(int32_t)x, (int32_t)y}, {width, height}};
		m_Impl->Cmd.setScissor(0, 1, &rect);
	}

	void* CommandList::GetNativeHandle()
	{
		return (void*)(VkCommandBuffer)m_Impl->Cmd;
	}

	// --------------------------------RenderDevice--------------------------------

	RenderDevice& RenderDevice::Get()
	{
		static RenderDevice s_Instance;
		return s_Instance;
	}

	void RenderDevice::Init(void* nativeWindow)
	{
		auto impl = std::make_unique<Impl>();
		impl->Window = (GLFWwindow*)nativeWindow;

		if (auto result = impl->Swapchain.Init(impl->Window); !result)
		{
			// Leave m_Impl null rather than half-built: every entry point below treats
			// that as "no device" instead of dereferencing an uninitialised frame ring.
			TF_CORE_ERROR("Swapchain init failed: {0}", result.error());
			impl->Swapchain.Shutdown();
			return;
		}

		impl->Frames.Init(impl->Swapchain.GetImageCount());
		impl->List.m_Impl = &impl->ListImpl;

		m_Impl = impl.release();
	}

	const Limits& RenderDevice::GetLimits() const
	{
		return VulkanContext::Get().GetLimits();
	}

	CommandList* RenderDevice::BeginFrame()
	{
		if (!m_Impl)
			return nullptr;

		auto& ctx = VulkanContext::Get();
		auto device = ctx.GetDevice();

		const uint64_t signalValue = m_Impl->Frames.WaitForSlot();
		auto& frame = m_Impl->Frames.Current();

		uint32_t imageIndex = 0;
		if (!m_Impl->Swapchain.AcquireNext(frame.ImageAvailable, imageIndex))
		{
			m_Impl->Frames.AbandonFrame();
			(void)m_Impl->Swapchain.Recreate();
			m_Impl->Frames.OnSwapchainResized(m_Impl->Swapchain.GetImageCount());
			return nullptr;
		}

		device.resetCommandPool(frame.Pool);

		m_Impl->DeletionQueue.Flush(m_Impl->Frames.CompletedValue());

		(void)frame.Cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		m_Impl->ImageIndex = imageIndex;
		m_Impl->FrameSignalValue = signalValue;
		m_Impl->ListImpl.Cmd = frame.Cmd;
		m_Impl->ListImpl.TargetImage = m_Impl->Swapchain.GetImage(imageIndex);
		m_Impl->ListImpl.TargetView = m_Impl->Swapchain.GetImageView(imageIndex);
		m_Impl->ListImpl.TargetExtent = m_Impl->Swapchain.GetExtent();
		m_Impl->ListImpl.TargetLayout = &m_Impl->Swapchain.GetImageLayout(imageIndex);
		m_Impl->FrameActive = true;

		return &m_Impl->List;
	}

	void RenderDevice::EndFrame()
	{
		if (!m_Impl || !m_Impl->FrameActive)
			return;

		auto& ctx = VulkanContext::Get();
		auto& frame = m_Impl->Frames.Current();

		// Tracked: a frame that opened no pass never reached ColorAttachmentOptimal.
		// dstStage must match the stage RenderFinished is signalled at, or present can begin
		// before this transition completes.
		TransitionImage(frame.Cmd, m_Impl->ListImpl.TargetImage, *m_Impl->ListImpl.TargetLayout, vk::ImageLayout::ePresentSrcKHR,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone);

		*m_Impl->ListImpl.TargetLayout = vk::ImageLayout::ePresentSrcKHR;

		(void)frame.Cmd.end();

		const vk::Semaphore renderFinished = m_Impl->Frames.RenderFinished(m_Impl->ImageIndex);

		const vk::SemaphoreSubmitInfo waits[]{
			{.semaphore = frame.ImageAvailable, .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput}};

		const vk::SemaphoreSubmitInfo signals[]{
			{.semaphore = renderFinished, .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput},
			{.semaphore = m_Impl->Frames.Timeline(),
				.value = m_Impl->FrameSignalValue,
				.stageMask = vk::PipelineStageFlagBits2::eAllCommands}};

		const vk::CommandBufferSubmitInfo cmdInfo{.commandBuffer = frame.Cmd};

		(void)ctx.GetGraphicsQueue().submit2({vk::SubmitInfo2{.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = waits,
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdInfo,
			.signalSemaphoreInfoCount = 2,
			.pSignalSemaphoreInfos = signals}});

		if (!m_Impl->Swapchain.Present(ctx.GetGraphicsQueue(), renderFinished, m_Impl->ImageIndex))
		{
			(void)m_Impl->Swapchain.Recreate(); // eSuboptimalKHR or eErrorOutOfDateKHR
			m_Impl->Frames.OnSwapchainResized(m_Impl->Swapchain.GetImageCount());
		}

		m_Impl->FrameActive = false;
	}

	void RenderDevice::OnResize(uint32_t, uint32_t)
	{
		if (!m_Impl)
			return;

		(void)m_Impl->Swapchain.Recreate();
		m_Impl->Frames.OnSwapchainResized(m_Impl->Swapchain.GetImageCount());
	}

	void RenderDevice::SetVSync(bool enabled)
	{
		if (!m_Impl)
			return;

		m_Impl->Swapchain.SetVSync(enabled); // rebuilds only when the flag actually changed
		m_Impl->Frames.OnSwapchainResized(m_Impl->Swapchain.GetImageCount());
	}

	void RenderDevice::WaitIdle()
	{
		(void)VulkanContext::Get().GetDevice().waitIdle();
	}

	void RenderDevice::Shutdown()
	{
		if (!m_Impl)
			return;

		WaitIdle();
		m_Impl->DeletionQueue.FlushAll();
		m_Impl->Frames.Shutdown();
		m_Impl->Swapchain.Shutdown();
		delete m_Impl;
		m_Impl = nullptr;
	}

	CommandList* RenderDevice::GetCurrentCommandList()
	{
		return m_Impl && m_Impl->FrameActive ? &m_Impl->List : nullptr;
	}

	uint32_t RenderDevice::GetSwapchainFormat() const
	{
		return m_Impl ? (uint32_t)m_Impl->Swapchain.GetFormat() : 0;
	}

	uint32_t RenderDevice::GetSwapchainImageCount() const
	{
		return m_Impl ? m_Impl->Swapchain.GetImageCount() : 0;
	}

	uint32_t RenderDevice::GetSwapchainMinImageCount() const
	{
		return m_Impl ? m_Impl->Swapchain.GetMinImageCount() : 0;
	}
}
