#include "tfpch.h"

#include "Timefall/RHI/Texture.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/GpuBuffer.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanRHIImpl.h"
#include "Platform/Vulkan/VulkanUploadContext.h"
#include "Platform/Vulkan/VulkanBindlessTable.h"
#include "Platform/Vulkan/GPUMemoryTracker.h"

namespace Timefall::RHI
{
	namespace
	{
		std::atomic<uint32_t> s_NextTrackerId{1};

		struct FormatCaps
		{
			bool HostCopy = false;
			bool LinearBlit = false;
		};

		const FormatCaps& GetFormatCaps(vk::Format format)
		{
			static std::unordered_map<vk::Format, FormatCaps> s_Cache;

			if (auto it = s_Cache.find(format); it != s_Cache.end())
				return it->second;

			const auto props =
				VulkanContext::Get().GetPhysicalDevice().getFormatProperties2<vk::FormatProperties2, vk::FormatProperties3>(format);
			const vk::FormatFeatureFlags2 optimal = props.get<vk::FormatProperties3>().optimalTilingFeatures;

			const FormatCaps caps{.HostCopy = (bool)(optimal & vk::FormatFeatureFlagBits2::eHostImageTransfer),
				.LinearBlit = (bool)(optimal & vk::FormatFeatureFlagBits2::eSampledImageFilterLinear)
					&& (bool)(optimal & vk::FormatFeatureFlagBits2::eBlitSrc) && (bool)(optimal & vk::FormatFeatureFlagBits2::eBlitDst)};

			TF_CORE_INFO("Texture format {0}: host copy {1}, linear blit {2}", vk::to_string(format), caps.HostCopy, caps.LinearBlit);
			return s_Cache.emplace(format, caps).first->second;
		}

		vk::ImageLayout HostCopyDstLayout()
		{
			static const vk::ImageLayout s_Layout = []() {
				auto physicalDevice = VulkanContext::Get().GetPhysicalDevice();

				vk::PhysicalDeviceHostImageCopyProperties hostProps{};
				vk::PhysicalDeviceProperties2 props{.pNext = &hostProps};
				physicalDevice.getProperties2(&props);

				std::vector<vk::ImageLayout> dstLayouts(hostProps.copyDstLayoutCount);
				hostProps.pCopyDstLayouts = dstLayouts.data();
				physicalDevice.getProperties2(&props);

				return std::ranges::contains(dstLayouts, vk::ImageLayout::eTransferDstOptimal) ? vk::ImageLayout::eTransferDstOptimal
																							   : vk::ImageLayout::eGeneral;
			}();

			return s_Layout;
		}

		vk::Format SRGBCounterpart(vk::Format format)
		{
			switch (format)
			{
				case vk::Format::eR8G8B8A8Unorm: return vk::Format::eR8G8B8A8Srgb;
				case vk::Format::eB8G8R8A8Unorm: return vk::Format::eB8G8R8A8Srgb;
				default: return vk::Format::eUndefined;
			}
		}

		uint32_t FullMipChain(uint32_t width, uint32_t height)
		{
			return (uint32_t)std::floor(std::log2((float)std::max(width, height))) + 1;
		}

		// Falls back to something identifiable: an unnamed image is a bare handle in validation output.
		std::string ResolveName(const TextureDesc& desc, vk::Format format)
		{
			return desc.DebugName ? desc.DebugName : std::format("Texture[{}x{} {}]", desc.Width, desc.Height, vk::to_string(format));
		}

		vk::ImageSubresourceRange ColorRange(uint32_t mipLevels)
		{
			return {vk::ImageAspectFlagBits::eColor, 0, mipLevels, 0, 1};
		}

		void RecordMipChain(
			vk::Image image, vk::Format format, uint32_t width, uint32_t height, uint32_t mipLevels, vk::ImageLayout level0Layout)
		{
			const vk::Filter filter = GetFormatCaps(format).LinearBlit ? vk::Filter::eLinear : vk::Filter::eNearest;

			VulkanUploadContext::Record([=](vk::CommandBuffer cmd) {
				auto transition = [&cmd, image](uint32_t level, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
									  vk::AccessFlags2 srcAccess, vk::AccessFlags2 dstAccess) {
					// AllTransfer, not Blit: level 0's incoming write comes from the upload copy, not a blit
					const vk::ImageMemoryBarrier2 barrier{.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
						.srcAccessMask = srcAccess,
						.dstStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
						.dstAccessMask = dstAccess,
						.oldLayout = oldLayout,
						.newLayout = newLayout,
						.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
						.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
						.image = image,
						.subresourceRange = {vk::ImageAspectFlagBits::eColor, level, 1, 0, 1}};

					cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
				};

				transition(0, level0Layout, vk::ImageLayout::eTransferSrcOptimal, vk::AccessFlagBits2::eTransferWrite,
					vk::AccessFlagBits2::eTransferRead);

				int32_t levelWidth = (int32_t)width;
				int32_t levelHeight = (int32_t)height;

				for (uint32_t level = 1; level < mipLevels; level++)
				{
					const int32_t nextWidth = std::max(levelWidth / 2, 1);
					const int32_t nextHeight = std::max(levelHeight / 2, 1);

					transition(level, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::AccessFlagBits2::eNone,
						vk::AccessFlagBits2::eTransferWrite);

					const vk::ImageBlit2 blit{.srcSubresource = {vk::ImageAspectFlagBits::eColor, level - 1, 0, 1},
						.srcOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{levelWidth, levelHeight, 1}}},
						.dstSubresource = {vk::ImageAspectFlagBits::eColor, level, 0, 1},
						.dstOffsets = {{vk::Offset3D{0, 0, 0}, vk::Offset3D{nextWidth, nextHeight, 1}}}};

					cmd.blitImage2({.srcImage = image,
						.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
						.dstImage = image,
						.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
						.regionCount = 1,
						.pRegions = &blit,
						.filter = filter});

					transition(level, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal,
						vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eTransferRead);

					levelWidth = nextWidth;
					levelHeight = nextHeight;
				}

				// TransferRead only: TRANSFER_SRC_OPTIMAL admits no other access, and the per-level barriers
				// already made every blit write available — this chains off their TransferRead second scope
				const vk::ImageMemoryBarrier2 toRead{.srcStageMask = vk::PipelineStageFlagBits2::eAllTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
					.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
					.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
					.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = image,
					.subresourceRange = ColorRange(mipLevels)};

				cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toRead});
			});
		}
	}

	Ref<Texture> Texture::Create(const TextureDesc& desc)
	{
		TF_PROFILE_FUNCTION();
		TF_CORE_ASSERT(desc.Width > 0 && desc.Height > 0, "Texture needs non-zero dimensions");

		Ref<Texture> texture(new Texture());
		texture->m_Impl = new Impl();
		Impl& impl = *texture->m_Impl;

		const vk::Format format = ToVkFormat(desc.PixelFormat);
		if (format == vk::Format::eUndefined)
		{
			TF_CORE_ERROR("Texture '{0}' has no Vulkan format mapping", desc.DebugName ? desc.DebugName : "<unnamed>");
			return texture;
		}

		impl.Format = format;
		impl.PixelFormat = desc.PixelFormat;
		impl.Width = desc.Width;
		impl.Height = desc.Height;

		const bool isAttachment = HasFlag(desc.Usage, TextureUsage::ColorAttachment) || HasFlag(desc.Usage, TextureUsage::DepthAttachment);
		impl.IsAttachment = isAttachment;

		impl.Aspect = IsDepthFormat(desc.PixelFormat) ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor;

		impl.MipLevels = isAttachment ? 1 : (desc.MipLevels == 0 ? FullMipChain(desc.Width, desc.Height) : desc.MipLevels);

		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eSampled;
		if (HasFlag(desc.Usage, TextureUsage::ColorAttachment))
			usage |= vk::ImageUsageFlagBits::eColorAttachment;
		if (HasFlag(desc.Usage, TextureUsage::DepthAttachment))
			usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;

		if (!isAttachment)
		{
			const FormatCaps& caps = GetFormatCaps(format);
			impl.HostCopyable = caps.HostCopy;

			usage |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
			if (caps.HostCopy)
				usage |= vk::ImageUsageFlagBits::eHostTransfer;
		}

		const vk::Format srgbFormat = SRGBCounterpart(format);
		const bool wantsSRGBView = desc.SRGBView && srgbFormat != vk::Format::eUndefined && !isAttachment;

		const vk::Format viewFormats[]{format, srgbFormat};
		const vk::ImageFormatListCreateInfo formatList{.viewFormatCount = 2, .pViewFormats = viewFormats};

		const vk::ImageCreateInfo imageInfo{.pNext = wantsSRGBView ? &formatList : nullptr,
			.flags = wantsSRGBView ? vk::ImageCreateFlagBits::eMutableFormat : vk::ImageCreateFlags{},
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = {desc.Width, desc.Height, 1},
			.mipLevels = impl.MipLevels,
			.arrayLayers = 1,
			.samples = vk::SampleCountFlagBits::e1,
			.tiling = vk::ImageTiling::eOptimal,
			.usage = usage,
			.sharingMode = vk::SharingMode::eExclusive,
			.initialLayout = vk::ImageLayout::eUndefined};

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

		VkImage raw = VK_NULL_HANDLE;
		VmaAllocationInfo allocated{};
		const VkResult result = vmaCreateImage(VulkanContext::Get().GetAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo),
			&allocInfo, &raw, &impl.Allocation, &allocated);
		if (result != VK_SUCCESS)
		{
			TF_CORE_ERROR("vmaCreateImage failed for '{0}' ({1}x{2}): {3}", desc.DebugName ? desc.DebugName : "<unnamed>", desc.Width,
				desc.Height, vk::to_string((vk::Result)result));
			return texture;
		}

		impl.Image = raw;

		const std::string name = ResolveName(desc, format);
		VulkanContext::Get().SetObjectName(impl.Image, name);
		vmaSetAllocationName(VulkanContext::Get().GetAllocator(), impl.Allocation, name.c_str());

		auto makeView = [&](vk::Format viewFormat, std::string_view suffix) -> vk::ImageView {
			auto view = VulkanContext::Get().GetDevice().createImageView({.image = impl.Image,
				.viewType = vk::ImageViewType::e2D,
				.format = viewFormat, .subresourceRange = {impl.Aspect, 0, impl.MipLevels, 0, 1}});

			if (!view)
			{
				TF_CORE_ERROR("createImageView failed for '{0}:{1}': {2}", name, suffix, vk::to_string(view.error()));
				return nullptr;
			}

			VulkanContext::Get().SetObjectName(*view, std::format("{}:{}", name, suffix));
			return *view;
		};

		impl.View = makeView(format, "View");
		if (wantsSRGBView)
			impl.SRGBView = makeView(srgbFormat, "SRGBView");

		impl.TrackerId = s_NextTrackerId++;
		GPUMemoryTracker::Track(isAttachment ? GPUMemCategory::Framebuffers : GPUMemCategory::Textures, impl.TrackerId, allocated.size);

		if (isAttachment)
			return texture;

		if (impl.HostCopyable)
		{
			const vk::HostImageLayoutTransitionInfo transition{.image = impl.Image,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.subresourceRange = ColorRange(impl.MipLevels)};

			(void)VulkanContext::Get().GetDevice().transitionImageLayout(transition);
		}
		else
		{
			const vk::ImageMemoryBarrier2 barrier{.srcStageMask = vk::PipelineStageFlagBits2::eNone,
				.srcAccessMask = vk::AccessFlagBits2::eNone,
				.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = impl.Image,
				.subresourceRange = ColorRange(impl.MipLevels)};

			VulkanUploadContext::Record([&barrier](vk::CommandBuffer cmd) {
				cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
			});
		}

		impl.CurrentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		return texture;
	}

	Ref<Texture> Texture::CreateWithData(const TextureDesc& desc, const void* data, uint64_t size)
	{
		TF_PROFILE_FUNCTION();

		Ref<Texture> texture = Create(desc);
		if (texture->IsValid())
			texture->Upload(data, size);

		return texture;
	}

	bool Texture::Upload(const void* data, uint64_t size)
	{
		TF_PROFILE_FUNCTION();

		if (!IsValid() || !data)
		{
			TF_CORE_ERROR("Texture::Upload on an invalid texture, or with no data");
			return false;
		}

		if (m_Impl->IsAttachment)
		{
			TF_CORE_ERROR("Texture::Upload on a render target attachment - attachments are rendered into, not uploaded");
			return false;
		}

		Impl& impl = *m_Impl;
		const uint64_t expected = (uint64_t)impl.Width * impl.Height * BytesPerPixel(impl.PixelFormat);
		if (size != expected)
		{
			TF_CORE_ERROR("Texture::Upload got {0} bytes for a {1}x{2} {3} image needing {4} - the RHI converts nothing", size, impl.Width,
				impl.Height, vk::to_string(impl.Format), expected);
			return false;
		}

		UploadScope scope;
		if (impl.HostCopyable)
		{
			const vk::ImageLayout copyLayout = HostCopyDstLayout();
			const vk::HostImageLayoutTransitionInfo toCopy{.image = impl.Image,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = copyLayout,
				.subresourceRange = ColorRange(impl.MipLevels)};

			(void)VulkanContext::Get().GetDevice().transitionImageLayout(toCopy);

			const vk::MemoryToImageCopy region{.pHostPointer = data,
				.memoryRowLength = 0,
				.memoryImageHeight = 0,
				.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
				.imageOffset = {0, 0, 0},
				.imageExtent = {impl.Width, impl.Height, 1}};

			if (auto result = VulkanContext::Get().GetDevice().copyMemoryToImage(
					{.dstImage = impl.Image, .dstImageLayout = copyLayout, .regionCount = 1, .pRegions = &region});
				!result)
			{
				TF_CORE_ERROR("vkCopyMemoryToImage failed: {0}", vk::to_string(result.error()));
				return false;
			}

			if (impl.MipLevels == 1)
			{
				const vk::HostImageLayoutTransitionInfo toRead{.image = impl.Image,
					.oldLayout = copyLayout,
					.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					.subresourceRange = ColorRange(impl.MipLevels)};

				(void)VulkanContext::Get().GetDevice().transitionImageLayout(toRead);
				return true;
			}

			RecordMipChain(impl.Image, impl.Format, impl.Width, impl.Height, impl.MipLevels, copyLayout);
			return true;
		}

		if (!VulkanUploadContext::UploadImage(impl.Image, impl.Width, impl.Height, BytesPerPixel(impl.PixelFormat), data, size))
			return false;

		if (impl.MipLevels == 1)
		{
			VulkanUploadContext::Record([&impl](vk::CommandBuffer cmd) {
				const vk::ImageMemoryBarrier2 toRead{.srcStageMask = vk::PipelineStageFlagBits2::eCopy,
					.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderSampledRead,
					.oldLayout = vk::ImageLayout::eTransferDstOptimal,
					.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = impl.Image,
					.subresourceRange = ColorRange(impl.MipLevels)};

				cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &toRead});
			});

			return true;
		}

		RecordMipChain(impl.Image, impl.Format, impl.Width, impl.Height, impl.MipLevels, vk::ImageLayout::eTransferDstOptimal);
		return true;
	}

	Texture::~Texture()
	{
		if (!m_Impl)
			return;

		if (m_Impl->Image)
		{
			GPUMemoryTracker::Untrack(m_Impl->IsAttachment ? GPUMemCategory::Framebuffers : GPUMemCategory::Textures, m_Impl->TrackerId);
			RenderDevice::Get().DeferDestroy(
				[image = m_Impl->Image, view = m_Impl->View, srgbView = m_Impl->SRGBView, allocation = m_Impl->Allocation,
					uiHandle = m_Impl->UIHandle, uiDestroy = m_Impl->UIHandleDestroy, bindless = m_Impl->BindlessIndex, bindlessSRGB = m_Impl->BindlessSRGBIndex]() {

					VulkanBindlessTable::Release(bindless);
					VulkanBindlessTable::Release(bindlessSRGB);

					if (uiHandle && uiDestroy)
						uiDestroy(uiHandle);

					auto device = VulkanContext::Get().GetDevice();
					if (srgbView)
						device.destroyImageView(srgbView);
					if (view)
						device.destroyImageView(view);

					vmaDestroyImage(VulkanContext::Get().GetAllocator(), (VkImage)image, allocation);
				});
		}

		delete m_Impl;
		m_Impl = nullptr;
	}

	uint32_t Texture::GetWidth() const
	{
		return m_Impl ? m_Impl->Width : 0;
	}

	uint32_t Texture::GetHeight() const
	{
		return m_Impl ? m_Impl->Height : 0;
	}

	uint32_t Texture::GetMipLevels() const
	{
		return m_Impl ? m_Impl->MipLevels : 0;
	}

	Format Texture::GetPixelFormat() const
	{
		return m_Impl ? m_Impl->PixelFormat : Format::Undefined;
	}

	bool Texture::IsValid() const
	{
		return m_Impl && m_Impl->Image && m_Impl->View;
	}

	void* Texture::GetNativeView(bool srgb) const
	{
		if (!m_Impl)
			return nullptr;

		const vk::ImageView view = srgb && m_Impl->SRGBView ? m_Impl->SRGBView : m_Impl->View;
		return (void*)(VkImageView)view;
	}

	uint32_t Texture::GetBindlessIndex(bool srgb)
	{
		if (!IsValid())
			return VulkanBindlessTable::WhiteIndex;

		const bool wantsSRGB = srgb && m_Impl->SRGBView;
		uint32_t& slot = wantsSRGB ? m_Impl->BindlessSRGBIndex : m_Impl->BindlessIndex;

		if (slot != VulkanBindlessTable::InvalidIndex)
			return slot;

		const uint32_t index = VulkanBindlessTable::Acquire(wantsSRGB ? m_Impl->SRGBView : m_Impl->View);
		if (index == VulkanBindlessTable::InvalidIndex)
			return VulkanBindlessTable::WhiteIndex;

		slot = index;
		return slot;
	}

	void* Texture::GetUIHandle() const
	{
		return m_Impl ? m_Impl->UIHandle : nullptr;
	}

	void Texture::SetUIHandle(void* handle, void (*destroy)(void*))
	{
		if (!m_Impl)
			return;

		m_Impl->UIHandle = handle;
		m_Impl->UIHandleDestroy = destroy;
	}
}
