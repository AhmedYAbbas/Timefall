#include "tfpch.h"

#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/FrameAllocator.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanUploadContext.h"
#include "Platform/Vulkan/VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanFrameRing.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"
#include "Platform/Vulkan/VulkanRHIImpl.h"
#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanBindings.h"

#include "Timefall/Renderer/GPUProfiler.h"

namespace Timefall::RHI
{
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
		vk::PipelineStageFlags2 srcStage, vk::AccessFlags2 srcAccess, vk::PipelineStageFlags2 dstStage, vk::AccessFlags2 dstAccess, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor)
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
			.subresourceRange = {aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers}};

		cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
	}

	static vk::AttachmentLoadOp ToVkLoadOp(LoadOp op)
	{
		return op == LoadOp::Clear ? vk::AttachmentLoadOp::eClear : op == LoadOp::Load ? vk::AttachmentLoadOp::eLoad
								   : vk::AttachmentLoadOp::eDontCare;
	}

	static vk::AttachmentStoreOp ToVkStoreOp(StoreOp op)
	{
		return op == StoreOp::Store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
	}

	// --------------------------------CommandList--------------------------------
	void CommandList::BeginPass(const PassDesc& desc)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "BeginPass called while a pass is already open");

		auto& ctx = VulkanContext::Get();
		m_Impl->PassLabelPushed = ctx.IsDebugEnabled() && desc.DebugName;
		if (m_Impl->PassLabelPushed)
			m_Impl->Cmd.beginDebugUtilsLabelEXT({.pLabelName = desc.DebugName});

		vk::RenderingAttachmentInfo colorInfos[8]{};
		vk::ClearValue colorClears[8]{};
		uint32_t colorCount = 0;
		vk::Extent2D extent{};

		if (desc.Target)
		{
			TF_CORE_ASSERT(desc.Target->IsValid(), "BeginPass on an invalid RenderTarget");

			colorCount = desc.Target->GetColorCount();
			extent = {desc.Target->GetWidth(), desc.Target->GetHeight()};

			for (uint32_t i = 0; i < colorCount; i++)
			{
				const ColorTarget& target = desc.Color[i];
				Texture::Impl& attachment = *desc.Target->GetColor(i)->m_Impl;

				const vk::ImageLayout oldLayout = target.Load == LoadOp::Load ? attachment.CurrentLayout : vk::ImageLayout::eUndefined;

				TransitionImage(m_Impl->Cmd, attachment.Image, oldLayout, vk::ImageLayout::eColorAttachmentOptimal,
					vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead,
					vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead);

				attachment.CurrentLayout = vk::ImageLayout::eColorAttachmentOptimal;

				if (IsIntegerFormat(attachment.PixelFormat))
					colorClears[i].color.int32 = std::array{target.ClearInt[0], target.ClearInt[1], target.ClearInt[2], target.ClearInt[3]};
				else
					colorClears[i].color.float32 =
						std::array{target.ClearValue[0], target.ClearValue[1], target.ClearValue[2], target.ClearValue[3]};

				colorInfos[i] = {.imageView = attachment.View,
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = ToVkLoadOp(target.Load),
					.storeOp = ToVkStoreOp(target.Store),
					.clearValue = colorClears[i]};
			}
		}
		else
		{
			const ColorTarget& target = desc.Color[0];

			colorCount = 1;
			extent = m_Impl->SwapExtent;

			const vk::ImageLayout oldLayout = target.Load == LoadOp::Load ? *m_Impl->SwapLayout : vk::ImageLayout::eUndefined;

			TransitionImage(m_Impl->Cmd, m_Impl->SwapImage, oldLayout, vk::ImageLayout::eColorAttachmentOptimal,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone,
				vk::PipelineStageFlagBits2::eColorAttachmentOutput,
				vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead);

			*m_Impl->SwapLayout = vk::ImageLayout::eColorAttachmentOptimal;

			colorClears[0].color.float32 =
				std::array{target.ClearValue[0], target.ClearValue[1], target.ClearValue[2], target.ClearValue[3]};

			colorInfos[0] = {.imageView = m_Impl->SwapView,
				.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
				.loadOp = ToVkLoadOp(target.Load),
				.storeOp = ToVkStoreOp(target.Store),
				.clearValue = colorClears[0]};
		}

		vk::RenderingAttachmentInfo depthInfo{};
		const bool hasDepth = desc.Target && desc.Target->GetDepth();

		if (hasDepth)
		{
			Texture::Impl& attachment = *desc.Target->GetDepth()->m_Impl;

			const vk::ImageLayout oldLayout = desc.Depth.Load == LoadOp::Load ? attachment.CurrentLayout : vk::ImageLayout::eUndefined;

			TransitionImage(m_Impl->Cmd, attachment.Image, oldLayout, vk::ImageLayout::eDepthAttachmentOptimal,
				vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				vk::PipelineStageFlagBits2::eEarlyFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				vk::ImageAspectFlagBits::eDepth);

			attachment.CurrentLayout = vk::ImageLayout::eDepthAttachmentOptimal;

			vk::ClearValue depthClear{};
			depthClear.depthStencil = vk::ClearDepthStencilValue{.depth = desc.Depth.ClearDepth, .stencil = 0};

			depthInfo = {.imageView = attachment.View,
				.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
				.loadOp = ToVkLoadOp(desc.Depth.Load),
				.storeOp = ToVkStoreOp(desc.Depth.Store),
				.clearValue = depthClear};
		}

		m_Impl->Cmd.beginRendering({.renderArea = {{0, 0}, extent}, .layerCount = 1, .colorAttachmentCount = colorCount, .pColorAttachments = colorInfos, .pDepthAttachment = hasDepth ? &depthInfo : nullptr});

		m_Impl->InPass = true;
		m_Impl->PassTarget = desc.Target;
		m_Impl->PassColorCount = colorCount;

		SetViewport(0, 0, extent.width, extent.height);
		SetScissor(0, 0, extent.width, extent.height);
	}

	void CommandList::EndPass()
	{
		m_Impl->Cmd.endRendering();

		if (m_Impl->PassTarget)
		{
			for (uint32_t i = 0; i < m_Impl->PassColorCount; i++)
			{
				Texture::Impl& attachment = *m_Impl->PassTarget->GetColor(i)->m_Impl;

				TransitionImage(m_Impl->Cmd, attachment.Image, attachment.CurrentLayout, vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
					vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead);

				attachment.CurrentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			}
		}

		m_Impl->PassTarget = nullptr;
		m_Impl->PassColorCount = 0;
		m_Impl->InPass = false;

		if (m_Impl->PassLabelPushed)
		{
			m_Impl->Cmd.endDebugUtilsLabelEXT();
			m_Impl->PassLabelPushed = false;
		}
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

	void CommandList::SetLineWidth(float width)
	{
		m_Impl->Cmd.setLineWidth(VulkanContext::Get().ClampLineWidth(width));
	}

	void CommandList::BindPipeline(const GraphicsPipeline& pipeline)
	{
		if (!pipeline.IsValid())
			return;

		const auto& impl = *pipeline.m_Impl;

		m_Impl->Cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, impl.Pipeline);
		m_Impl->BoundLayout = impl.Layout;
		m_Impl->BoundPushStages = impl.PushStages;
		m_Impl->BoundPushSize = impl.PushSize;

		if (impl.HasGlobalPrefix)
			VulkanBindings::BindGlobalSets(m_Impl->Cmd, impl.Layout);
	}

	void CommandList::PushConstants(const void* data, uint32_t size, uint32_t offset)
	{
		TF_CORE_ASSERT(m_Impl->BoundLayout, "PushConstans before BindPipeline");
		TF_CORE_ASSERT(offset + size <= m_Impl->BoundPushSize, "Push constant write exceeds the shader's block");

		m_Impl->Cmd.pushConstants2(
			{.layout = m_Impl->BoundLayout, .stageFlags = m_Impl->BoundPushStages, .offset = offset, .size = size, .pValues = data});
	}

	void CommandList::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		m_Impl->Cmd.draw(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandList::BindVertexBuffer(const GpuBuffer& buffer, uint64_t offset)
	{
		if (!buffer.IsValid())
			return;

		const vk::Buffer handle = buffer.m_Impl->Buffer;
		const vk::DeviceSize vkOffset = offset;

		// bindVertexBuffers2: extended dynamic state, core in 1.3. Null sizes/strides keep the
		// pipeline's baked stride, which GraphicsPipelineDesc::VertexLayout already fixed.
		m_Impl->Cmd.bindVertexBuffers2(0, 1, &handle, &vkOffset, nullptr, nullptr);
	}

	void CommandList::BindIndexBuffer(const GpuBuffer& buffer, IndexType type, uint64_t offset)
	{
		if (!buffer.IsValid())
			return;

		// bindIndexBuffer2 (maintenance5) takes an explicit size; WholeSize means "to the end".
		m_Impl->Cmd.bindIndexBuffer2(
			buffer.m_Impl->Buffer, offset, vk::WholeSize, type == IndexType::U16 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
	}

	void CommandList::DrawIndexed(
		uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		m_Impl->Cmd.drawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void CommandList::CopyTexture(Texture& src, Texture& dst)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "CopyTexture must be called outside a pass");

		if (!src.IsValid() || !dst.IsValid())
			return;

		Texture::Impl& s = *src.m_Impl;
		Texture::Impl& d = *dst.m_Impl;

		if (s.Format != d.Format || s.Width != d.Width || s.Height != d.Height)
		{
			TF_CORE_ERROR("CopyTexture needs matching format and extent");
			return;
		}

		TransitionImage(m_Impl->Cmd, s.Image, s.CurrentLayout, vk::ImageLayout::eTransferSrcOptimal,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, s.Aspect);
		s.CurrentLayout = vk::ImageLayout::eTransferSrcOptimal;

		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead, vk::PipelineStageFlagBits2::eCopy,
			vk::AccessFlagBits2::eTransferWrite, d.Aspect);
		d.CurrentLayout = vk::ImageLayout::eTransferDstOptimal;

		const vk::ImageCopy2 region{.srcSubresource = {s.Aspect, 0, 0, 1},
			.srcOffset = {0, 0, 0},
			.dstSubresource = {d.Aspect, 0, 0, 1},
			.dstOffset = {0, 0, 0},
			.extent = {s.Width, s.Height, 1}};

		m_Impl->Cmd.copyImage2({.srcImage = s.Image, .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal, .dstImage = d.Image, .dstImageLayout = vk::ImageLayout::eTransferDstOptimal, .regionCount = 1, .pRegions = &region});

		TransitionImage(m_Impl->Cmd, s.Image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, s.Aspect);
		s.CurrentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, d.Aspect);
		d.CurrentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

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
		GPUProfiler::Init();
		VulkanUploadContext::Init();
		FrameAllocator::Init();
		VulkanSamplerCache::Init();
		VulkanBindings::Init();
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

		// Same slot arithmetic as VulkanFrameRing::Current(); WaitForSlot has already proven this
		// slot's previous frame retired, which is what makes the bump reset free.
		FrameAllocator::BeginFrame((uint32_t)(m_Impl->Frames.FrameValue() % FRAMES_IN_FLIGHT));
		VulkanBindings::BeginFrame((uint32_t)(m_Impl->Frames.FrameValue() % FRAMES_IN_FLIGHT));

		(void)frame.Cmd.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

		m_Impl->ImageIndex = imageIndex;
		m_Impl->FrameSignalValue = signalValue;
		m_Impl->ListImpl.Cmd = frame.Cmd;
		m_Impl->ListImpl.SwapImage = m_Impl->Swapchain.GetImage(imageIndex);
		m_Impl->ListImpl.SwapView = m_Impl->Swapchain.GetImageView(imageIndex);
		m_Impl->ListImpl.SwapExtent = m_Impl->Swapchain.GetExtent();
		m_Impl->ListImpl.SwapLayout = &m_Impl->Swapchain.GetImageLayout(imageIndex);
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
		TransitionImage(frame.Cmd, m_Impl->ListImpl.SwapImage, *m_Impl->ListImpl.SwapLayout, vk::ImageLayout::ePresentSrcKHR,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
			vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eNone);

		*m_Impl->ListImpl.SwapLayout = vk::ImageLayout::ePresentSrcKHR;

		GPUProfiler::Collect();
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

	void RenderDevice::DeferDestroy(std::function<void()>&& fn)
	{
		if (!m_Impl)
		{
			// Nothing is in flight, so run it now - unless the context is gone too, in which case the
			// object died with the device and freeing it would touch a destroyed allocator.
			if (VulkanContext::Get().GetDevice())
				fn();

			return;
		}

		m_Impl->DeletionQueue.Push(m_Impl->Frames.FrameValue(), std::move(fn));
	}

	Format RenderDevice::GetSwapchainColorFormat() const
	{
		if (!m_Impl)
			return Format::Undefined;

		switch (m_Impl->Swapchain.GetFormat())
		{
			case vk::Format::eB8G8R8A8Unorm: return Format::BGRA8Unorm;
			case vk::Format::eR8G8B8A8Unorm: return Format::RGBA8Unorm;
			default: return Format::Undefined;
		}
	}

	void RenderDevice::Shutdown()
	{
		if (!m_Impl)
			return;

		WaitIdle();
		GPUProfiler::Shutdown();

		// Order is load-bearing: FrameAllocator's slot buffers push their destroys into the queue,
		// so FlushAll must follow it or they leak; the upload context goes last because a queued
		// lambda may still be destroying a buffer it wrote into.
		FrameAllocator::Shutdown();
		VulkanBindings::Shutdown();
		VulkanSamplerCache::Shutdown();
		m_Impl->DeletionQueue.FlushAll();
		VulkanUploadContext::Shutdown();

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
