#include "tfpch.h"


#include "Timefall/RHI/Texture.h"
#include "Timefall/RHI/RenderDevice.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanRHIImpl.h"
#include "Platform/Vulkan/VulkanUploadContext.h"
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
				default:						 return vk::Format::eUndefined;
			}
		}

		uint32_t FullMipChain(uint32_t width, uint32_t height)
		{
			return (uint32_t)std::floor(std::log2((float)std::max(width, height))) + 1;
		}

		vk::ImageSubresourceRange WholeImage(const Texture& texture)
		{
			return {vk::ImageAspectFlagBits::eColor, 0, texture.GetMipLevels(), 0, 1};
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
		impl.MipLevels = desc.MipLevels == 0 ? FullMipChain(desc.Width, desc.Height) : desc.MipLevels;

		const FormatCaps& caps = GetFormatCaps(format);
		impl.HostCopyable = caps.HostCopy;

		// TransferSrc as well as TransferDst; The mip chain blits level N-1 into level N
		vk::ImageUsageFlags usage =
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
		if (caps.HostCopy)
			usage |= vk::ImageUsageFlagBits::eHostTransfer;

		const vk::Format srgbFormat = SRGBCounterpart(format);
		const bool wantsSRGBView = desc.SRGBView && srgbFormat != vk::Format::eUndefined;

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
		const VkResult result = vmaCreateImage(VulkanContext::Get().GetAllocator(), reinterpret_cast<const VkImageCreateInfo*>(&imageInfo), &allocInfo, &raw, &impl.Allocation, &allocated);
		if (result != VK_SUCCESS)
		{
			TF_CORE_ERROR("vmaCreateImage failed for '{0}' ({1}x{2}): {3}", desc.DebugName ? desc.DebugName : "<unnamed>", desc.Width,
				desc.Height, vk::to_string((vk::Result)result));
			return texture;
		}

		impl.Image = raw;

		auto makeView = [&](vk::Format viewFormat) -> vk::ImageView
		{
			auto view = VulkanContext::Get().GetDevice().createImageView(
				{.image = impl.Image, .viewType = vk::ImageViewType::e2D, .format = viewFormat, .subresourceRange = WholeImage(*texture.get())
			});

			if (!view)
			{
				TF_CORE_ERROR(
					"createImageView failed for '{0}': {1}", desc.DebugName ? desc.DebugName : "<unnamed>", vk::to_string(view.error()));
				return nullptr;
			}

			return *view;
		};

		impl.View = makeView(format);
		if (wantsSRGBView)
			impl.SRGBView = makeView(srgbFormat);

		if (desc.DebugName)
		{
			VulkanContext::Get().SetObjectName(impl.Image, desc.DebugName);
			if (impl.View)
				VulkanContext::Get().SetObjectName(impl.View, desc.DebugName);
		}

		impl.TrackerId = s_NextTrackerId++;
		GPUMemoryTracker::Track(GPUMemCategory::Textures, impl.TrackerId, allocated.size);

		if (impl.HostCopyable)
		{
			const vk::HostImageLayoutTransitionInfo transition{.image = impl.Image,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.subresourceRange = WholeImage(*texture.get())};

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
				.subresourceRange = WholeImage(*texture.get())};

			VulkanUploadContext::Record([&barrier](vk::CommandBuffer cmd)
			{
				cmd.pipelineBarrier2({.imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier});
			});
		}

		return texture;
	}

	Texture::~Texture()
	{
		if (!m_Impl)
			return;

		if (m_Impl->Image)
		{
			GPUMemoryTracker::Untrack(GPUMemCategory::Textures, m_Impl->TrackerId);
			RenderDevice::Get().DeferDestroy(
				[image = m_Impl->Image, view = m_Impl->View, srgbView = m_Impl->SRGBView, allocation = m_Impl->Allocation,
					uiHandle = m_Impl->UIHandle, uiDestroy = m_Impl->UIHandleDestroy]() {
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
