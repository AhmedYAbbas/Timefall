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

	static vk::ImageSubresourceRange WholeImage(vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor)
	{
		return {aspect, 0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers};
	}

	static void TransitionImage(vk::CommandBuffer cmd, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
		vk::PipelineStageFlags2 srcStage, vk::AccessFlags2 srcAccess, vk::PipelineStageFlags2 dstStage, vk::AccessFlags2 dstAccess, vk::ImageSubresourceRange range = WholeImage())
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
			.subresourceRange = range};

		cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
	}

	// what an image was last used for, inferred from the layout it is sitting in
	static std::pair<vk::PipelineStageFlags2, vk::AccessFlags2> LayoutSourceSync(vk::ImageLayout layout)
	{
		switch (layout)
		{
			case vk::ImageLayout::eColorAttachmentOptimal:
				return {vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite};
			case vk::ImageLayout::eDepthAttachmentOptimal:
				return {vk::PipelineStageFlagBits2::eLateFragmentTests, vk::AccessFlagBits2::eDepthStencilAttachmentWrite};
			case vk::ImageLayout::eShaderReadOnlyOptimal:
				return {vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead};
			case vk::ImageLayout::eTransferSrcOptimal:
				return {vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferRead};
			case vk::ImageLayout::eTransferDstOptimal:
				return {vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferWrite};
			default:
				return {vk::PipelineStageFlagBits2::eTopOfPipe, vk::AccessFlagBits2::eNone};
		}
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
			extent = {std::max(1u, desc.Target->GetWidth() >> desc.Mip), std::max(1u, desc.Target->GetHeight() >> desc.Mip)};

			for (uint32_t i = 0; i < colorCount; i++)
			{
				const ColorTarget& target = desc.Color[i];
				Texture::Impl& attachment = *desc.Target->GetColor(i)->m_Impl;

				const vk::ImageSubresourceRange range{attachment.Aspect, desc.Mip, 1, desc.Layer, 1};

				const vk::ImageLayout oldLayout = target.Load == LoadOp::Load ? attachment.LayoutAt(desc.Mip, desc.Layer) : vk::ImageLayout::eUndefined;

				TransitionImage(m_Impl->Cmd, attachment.Image, oldLayout, vk::ImageLayout::eColorAttachmentOptimal,
					vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead,
					vk::PipelineStageFlagBits2::eColorAttachmentOutput,
					vk::AccessFlagBits2::eColorAttachmentWrite | vk::AccessFlagBits2::eColorAttachmentRead, range);

				attachment.LayoutAt(desc.Mip, desc.Layer) = vk::ImageLayout::eColorAttachmentOptimal;

				if (IsIntegerFormat(attachment.PixelFormat))
					colorClears[i].color.int32 = std::array{target.ClearInt[0], target.ClearInt[1], target.ClearInt[2], target.ClearInt[3]};
				else
					colorClears[i].color.float32 =
						std::array{target.ClearValue[0], target.ClearValue[1], target.ClearValue[2], target.ClearValue[3]};

				colorInfos[i] = {.imageView = attachment.SubresourceView(desc.Mip, desc.Layer),
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

			const vk::ImageSubresourceRange range{attachment.Aspect, desc.Mip, 1, desc.Layer, 1};

			const vk::ImageLayout oldLayout =
				desc.Depth.Load == LoadOp::Load ? attachment.LayoutAt(desc.Mip, desc.Layer) : vk::ImageLayout::eUndefined;

			const bool fromShaderRead = oldLayout == vk::ImageLayout::eShaderReadOnlyOptimal;

			TransitionImage(m_Impl->Cmd, attachment.Image, oldLayout, vk::ImageLayout::eDepthAttachmentOptimal,
				fromShaderRead ? vk::PipelineStageFlagBits2::eFragmentShader : vk::PipelineStageFlagBits2::eLateFragmentTests, fromShaderRead ? vk::AccessFlagBits2::eShaderSampledRead : vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
				vk::PipelineStageFlagBits2::eEarlyFragmentTests,
				vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead,
				range);

			attachment.LayoutAt(desc.Mip, desc.Layer) = vk::ImageLayout::eDepthAttachmentOptimal;

			vk::ClearValue depthClear{};
			depthClear.depthStencil = vk::ClearDepthStencilValue{.depth = desc.Depth.ClearDepth, .stencil = 0};

			depthInfo = {.imageView = attachment.SubresourceView(desc.Mip, desc.Layer),
				.imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
				.loadOp = ToVkLoadOp(desc.Depth.Load),
				.storeOp = ToVkStoreOp(desc.Depth.Store),
				.clearValue = depthClear};
		}

		m_Impl->Cmd.beginRendering({.renderArea = {{0, 0}, extent}, .layerCount = 1, .colorAttachmentCount = colorCount, .pColorAttachments = colorInfos, .pDepthAttachment = hasDepth ? &depthInfo : nullptr});

		m_Impl->InPass = true;
		m_Impl->PassTarget = desc.Target;
		m_Impl->PassColorCount = colorCount;
		m_Impl->PassLayer = desc.Layer;
		m_Impl->PassMip = desc.Mip;
		m_Impl->PassSlice = 0;

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

				const vk::ImageSubresourceRange range{attachment.Aspect, m_Impl->PassMip, 1, m_Impl->PassLayer, 1};

				TransitionImage(m_Impl->Cmd, attachment.Image, attachment.LayoutAt(m_Impl->PassMip, m_Impl->PassLayer),
					vk::ImageLayout::eShaderReadOnlyOptimal,
					vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite,
					vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead, range);

				attachment.LayoutAt(m_Impl->PassMip, m_Impl->PassLayer) = vk::ImageLayout::eShaderReadOnlyOptimal;
			}

			if (m_Impl->PassTarget->GetDepth())
			{
				Texture::Impl& depth = *m_Impl->PassTarget->GetDepth()->m_Impl;

				const vk::ImageSubresourceRange range{depth.Aspect, m_Impl->PassMip, 1, m_Impl->PassLayer, 1};

				TransitionImage(m_Impl->Cmd, depth.Image, depth.LayoutAt(m_Impl->PassMip, m_Impl->PassLayer),
					vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits2::eLateFragmentTests,
					vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::PipelineStageFlagBits2::eFragmentShader, vk::AccessFlagBits2::eShaderSampledRead, range);

				depth.LayoutAt(m_Impl->PassMip, m_Impl->PassLayer) = vk::ImageLayout::eShaderReadOnlyOptimal;
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
			VulkanBindings::BindGlobalSets(m_Impl->Cmd, impl.Layout, m_Impl->PassSlice);
	}

	void CommandList::SetPassUniformSlice(uint32_t slice)
	{
		m_Impl->PassSlice = slice;
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

		if (!(s.UsageFlags & vk::ImageUsageFlagBits::eTransferSrc) || !(d.UsageFlags & vk::ImageUsageFlagBits::eTransferDst))
		{
			TF_CORE_ERROR("CopyTexture needs TextureUsage::TransferSrc on the source and TextureUsage::TransferDst on the destination");
			return;
		}

		const auto [srcStage, srcAccess] = LayoutSourceSync(s.LayoutAt(0, 0));

		TransitionImage(m_Impl->Cmd, s.Image, s.LayoutAt(0, 0), vk::ImageLayout::eTransferSrcOptimal, srcStage, srcAccess,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, s.FullRange());
		s.SetAllLayouts(vk::ImageLayout::eTransferSrcOptimal);

		// the copy rewrites every texel, so the old contents are discarded; only the write-after-read against
		// whatever last sampled the destination needs ordering, and the execution dependency alone gives that
		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			LayoutSourceSync(d.LayoutAt(0, 0)).first, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy,
			vk::AccessFlagBits2::eTransferWrite, d.FullRange());
		d.SetAllLayouts(vk::ImageLayout::eTransferDstOptimal);

		const vk::ImageCopy2 region{.srcSubresource = {s.Aspect, 0, 0, 1},
			.srcOffset = {0, 0, 0},
			.dstSubresource = {d.Aspect, 0, 0, 1},
			.dstOffset = {0, 0, 0},
			.extent = {s.Width, s.Height, 1}};

		m_Impl->Cmd.copyImage2({.srcImage = s.Image, .srcImageLayout = vk::ImageLayout::eTransferSrcOptimal, .dstImage = d.Image, .dstImageLayout = vk::ImageLayout::eTransferDstOptimal, .regionCount = 1, .pRegions = &region});

		TransitionImage(m_Impl->Cmd, s.Image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, s.FullRange());
		s.SetAllLayouts(vk::ImageLayout::eShaderReadOnlyOptimal);

		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, d.FullRange());
		d.SetAllLayouts(vk::ImageLayout::eShaderReadOnlyOptimal);
	}

	void CommandList::CopyTextureToBuffer(Texture& src, GpuBuffer& dst, const TextureRegion& region, uint64_t dstOffset)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "CopyTextureToBuffer must be called outside a pass");

		if (!src.IsValid() || !dst.IsValid())
			return;

		Texture::Impl& s = *src.m_Impl;

		if (region.Mip >= s.MipLevels || region.Layer >= s.ArrayLayers)
		{
			TF_CORE_ERROR("CopyTextureToBuffer got mip {0} layer {1} on a {2}-mip {3}-layer image", region.Mip, region.Layer, s.MipLevels,
				s.ArrayLayers);
			return;
		}

		if (!(s.UsageFlags & vk::ImageUsageFlagBits::eTransferSrc))
		{
			TF_CORE_ERROR("CopyTextureToBuffer needs TextureUsage::TransferSrc on the source");
			return;
		}

		const uint32_t width = std::max(1u, s.Width >> region.Mip);
		const uint32_t height = std::max(1u, s.Height >> region.Mip);
		const uint64_t bytes = (uint64_t)width * height * BytesPerPixel(s.PixelFormat);

		if (dstOffset + bytes > dst.Size())
		{
			TF_CORE_ERROR("CopyTextureToBuffer of {0} bytes at offset {1} exceeds the {2} byte buffer", bytes, dstOffset, dst.Size());
			return;
		}

		const vk::ImageSubresourceRange range{s.Aspect, region.Mip, 1, region.Layer, 1};
		const vk::ImageLayout oldLayout = s.LayoutAt(region.Mip, region.Layer);
		const auto [srcStage, srcAccess] = LayoutSourceSync(oldLayout);

		TransitionImage(m_Impl->Cmd, s.Image, oldLayout, vk::ImageLayout::eTransferSrcOptimal, srcStage, srcAccess,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, range);
		s.LayoutAt(region.Mip, region.Layer) = vk::ImageLayout::eTransferSrcOptimal;

		const vk::BufferImageCopy2 copy{.bufferOffset = dstOffset,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {s.Aspect, region.Mip, region.Layer, 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1}};

		m_Impl->Cmd.copyImageToBuffer2({.srcImage = s.Image,
			.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
			.dstBuffer = dst.m_Impl->Buffer,
			.regionCount = 1,
			.pRegions = &copy});

		TransitionImage(m_Impl->Cmd, s.Image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, range);
		s.LayoutAt(region.Mip, region.Layer) = vk::ImageLayout::eShaderReadOnlyOptimal;
	}

	void CommandList::CopyBufferToTexture(const GpuBuffer& src, Texture& dst, const TextureRegion& region, uint64_t srcOffset)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "CopyBufferToTexture must be called outside a pass");

		if (!src.IsValid() || !dst.IsValid())
			return;

		Texture::Impl& d = *dst.m_Impl;

		if (region.Mip >= d.MipLevels || region.Layer >= d.ArrayLayers)
		{
			TF_CORE_ERROR("CopyBufferToTexture got mip {0} layer {1} on a {2}-mip {3}-layer image", region.Mip, region.Layer, d.MipLevels,
				d.ArrayLayers);
			return;
		}

		if (!(d.UsageFlags & vk::ImageUsageFlagBits::eTransferDst))
		{
			TF_CORE_ERROR("CopyBufferToTexture needs TextureUsage::TransferDst on the destination");
			return;
		}

		const uint32_t width = std::max(1u, d.Width >> region.Mip);
		const uint32_t height = std::max(1u, d.Height >> region.Mip);
		const uint64_t bytes = (uint64_t)width * height * BytesPerPixel(d.PixelFormat);

		if (srcOffset + bytes > src.Size())
		{
			TF_CORE_ERROR("CopyBufferToTexture of {0} bytes at offset {1} exceeds the {2} byte buffer", bytes, srcOffset, src.Size());
			return;
		}

		const vk::ImageSubresourceRange range{d.Aspect, region.Mip, 1, region.Layer, 1};

		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
			LayoutSourceSync(d.LayoutAt(region.Mip, region.Layer)).first, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eCopy,
			vk::AccessFlagBits2::eTransferWrite, range);
		d.LayoutAt(region.Mip, region.Layer) = vk::ImageLayout::eTransferDstOptimal;

		const vk::BufferImageCopy2 copy{.bufferOffset = srcOffset,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource = {d.Aspect, region.Mip, region.Layer, 1},
			.imageOffset = {0, 0, 0},
			.imageExtent = {width, height, 1}};

		m_Impl->Cmd.copyBufferToImage2({.srcBuffer = src.m_Impl->Buffer,
			.dstImage = d.Image,
			.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
			.regionCount = 1,
			.pRegions = &copy});

		TransitionImage(m_Impl->Cmd, d.Image, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eCopy, vk::AccessFlagBits2::eTransferWrite, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, range);
		d.LayoutAt(region.Mip, region.Layer) = vk::ImageLayout::eShaderReadOnlyOptimal;
	}

	void CommandList::GenerateMips(Texture& texture)
	{
		TF_CORE_ASSERT(!m_Impl->InPass, "GenerateMips must be called outside a pass");

		if (!texture.IsValid())
			return;

		Texture::Impl& impl = *texture.m_Impl;
		if (impl.MipLevels <= 1)
			return;

		if (!(impl.UsageFlags & vk::ImageUsageFlagBits::eTransferSrc) || !(impl.UsageFlags & vk::ImageUsageFlagBits::eTransferDst))
		{
			TF_CORE_ERROR("GenerateMips needs TransferSrc and TransferDst usage on the texture");
			return;
		}

		const uint32_t layers = impl.ArrayLayers;
		const vk::Filter filter = impl.LinearBlit ? vk::Filter::eLinear : vk::Filter::eNearest;

		// mip 0 arrives from whatever last wrote it (an attachment write, an upload); the rest of the
		// chain is transfer-only, so the source sync has to come from the layout each level sits in
		const auto transition = [&](uint32_t mip, vk::ImageLayout newLayout, vk::AccessFlags2 dstAccess)
		{
			const vk::ImageLayout oldLayout = impl.LayoutAt(mip, 0);
			const auto [srcStage, srcAccess] = LayoutSourceSync(oldLayout);

			TransitionImage(m_Impl->Cmd, impl.Image, oldLayout, newLayout, srcStage, srcAccess,
				vk::PipelineStageFlagBits2::eAllTransfer, dstAccess, {impl.Aspect, mip, 1, 0, layers});

			for (uint32_t layer = 0; layer < layers; layer++)
				impl.LayoutAt(mip, layer) = newLayout;
		};

		transition(0, vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eTransferRead);

		int32_t levelWidth = (int32_t)impl.Width;
		int32_t levelHeight = (int32_t)impl.Height;

		for (uint32_t mip = 1; mip < impl.MipLevels; mip++)
		{
			const int32_t nextWidth = std::max(levelWidth / 2, 1);
			const int32_t nextHeight = std::max(levelHeight / 2, 1);

			transition(mip, vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eTransferWrite);

			const vk::ImageBlit2 blit{.srcSubresource = {impl.Aspect, mip - 1, 0, layers},
				.srcOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{levelWidth, levelHeight, 1}}},
				.dstSubresource = {impl.Aspect, mip, 0, layers},
				.dstOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{nextWidth, nextHeight, 1}}}};

			m_Impl->Cmd.blitImage2({.srcImage = impl.Image,
				.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
				.dstImage = impl.Image,
				.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
				.regionCount = 1,
				.pRegions = &blit,
				.filter = filter});

			transition(mip, vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eTransferRead);

			levelWidth = nextWidth;
			levelHeight = nextHeight;
		}

		TransitionImage(m_Impl->Cmd, impl.Image, vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::PipelineStageFlagBits2::eAllTransfer, vk::AccessFlagBits2::eTransferRead, vk::PipelineStageFlagBits2::eFragmentShader,
			vk::AccessFlagBits2::eShaderSampledRead, impl.FullRange());

		impl.SetAllLayouts(vk::ImageLayout::eShaderReadOnlyOptimal);
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

	void RenderDevice::ExecuteImmediate(const std::function<void(CommandList&)>& fn)
	{
		TF_PROFILE_FUNCTION();

		if (!fn)
			return;

		CommandList list;
		CommandList::Impl impl{};
		list.m_Impl = &impl;

		VulkanUploadContext::Begin();
		VulkanUploadContext::Record([&](vk::CommandBuffer cmd)
		{
			impl.Cmd = cmd;
			fn(list);
		});
		VulkanUploadContext::End();

		list.m_Impl = nullptr;
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
