#include "tfpch.h"
#include "VulkanSwapchain.h"
#include "Platform/Vulkan/VulkanContext.h"

#include <GLFW/glfw3.h>

namespace Timefall
{
	std::expected<void, std::string> VulkanSwapchain::Init(GLFWwindow* window)
	{
		m_Window = window;

		VkSurfaceKHR raw = VK_NULL_HANDLE;
		if (glfwCreateWindowSurface(VulkanContext::Get().GetInstance(), window, nullptr, &raw) != VK_SUCCESS)
			return std::unexpected("glfwCreateWindowSurface failed - was GLFW_CLIENT_API set to GLFW_NO_API?");

		m_Surface = raw;
		VulkanContext::Get().SetObjectName(m_Surface, "WindowSurface");

		auto supported =
			VulkanContext::Get().GetPhysicalDevice().getSurfaceSupportKHR(VulkanContext::Get().GetGraphicsQueueFamily(), m_Surface);
		if (!supported || !*supported)
			return std::unexpected("Graphics queue family cannot present to this surface");

		return Build();
	}

	std::expected<void, std::string> VulkanSwapchain::Build()
	{
		auto& ctx = VulkanContext::Get();
		const auto physical = ctx.GetPhysicalDevice();

		const vk::PhysicalDeviceSurfaceInfo2KHR surfaceInfo{.surface = m_Surface};

		auto caps2 = physical.getSurfaceCapabilities2KHR(surfaceInfo);
		if (!caps2)
			return std::unexpected("getSurfaceCapabilities2KHR failed");

		const auto& caps = caps2->surfaceCapabilities;
		if (caps.currentExtent.width == 0 || caps.currentExtent.height == 0)
		{
			m_Extent = vk::Extent2D{0, 0};
			return {};
		}

		auto formats = physical.getSurfaceFormats2KHR(surfaceInfo);
		if (!formats || formats->empty())
			return std::unexpected("No surface formats");

		vk::SurfaceFormatKHR chosen = (*formats)[0].surfaceFormat;
		for (const auto& format : *formats)
		{
			if (format.surfaceFormat.format == vk::Format::eB8G8R8A8Unorm
				&& format.surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				chosen = format.surfaceFormat;
				break;
			}
		}

		auto modes = physical.getSurfacePresentModesKHR(m_Surface);
		const auto has = [&](vk::PresentModeKHR m) { return modes && std::ranges::find(*modes, m) != modes->end(); };

		vk::PresentModeKHR present = vk::PresentModeKHR::eFifo;
		if (!m_VSync)
		{
			if (has(vk::PresentModeKHR::eMailbox))
				present = vk::PresentModeKHR::eMailbox;
			else if (has(vk::PresentModeKHR::eImmediate))
				present = vk::PresentModeKHR::eImmediate;
		}

		uint32_t imageCount = caps.minImageCount + 1;
		if (caps.maxImageCount > 0)
			imageCount = std::min(imageCount, caps.maxImageCount);

		const vk::SwapchainKHR old = m_Swapchain;

		auto swapchain = ctx.GetDevice().createSwapchainKHR({.surface = m_Surface,
			.minImageCount = imageCount,
			.imageFormat = chosen.format,
			.imageColorSpace = chosen.colorSpace,
			.imageExtent = caps.currentExtent,
			.imageArrayLayers = 1,
			.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
			.imageSharingMode = vk::SharingMode::eExclusive,
			.preTransform = caps.currentTransform,
			.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
			.presentMode = present,
			.clipped = VK_TRUE,
			.oldSwapchain = old});

		if (!swapchain)
			return std::unexpected(std::format("createSwapchainKHR failed {}", vk::to_string(swapchain.error())));

		DestroyViews();
		if (old)
			ctx.GetDevice().destroySwapchainKHR(old);

		m_Swapchain = *swapchain;
		ctx.SetObjectName(m_Swapchain, "Swapchain");

		m_Format = chosen.format;
		m_Extent = caps.currentExtent;
		m_MinImageCount = caps.minImageCount;

		auto images = ctx.GetDevice().getSwapchainImagesKHR(m_Swapchain);
		if (!images)
			return std::unexpected("getSwapchainImagesKHR failed");

		m_Images = *images;

		// Fresh images: contents and layout are undefined until the first pass writes them.
		m_ImageLayouts.assign(m_Images.size(), vk::ImageLayout::eUndefined);

		m_ImageViews.resize(m_Images.size());
		for (size_t i = 0; i < m_Images.size(); i++)
		{
			auto view = ctx.GetDevice().createImageView({.image = m_Images[i],
				.viewType = vk::ImageViewType::e2D,
				.format = m_Format,
				.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

			if (!view)
				return std::unexpected("createImageView failed");

			m_ImageViews[i] = *view;

			ctx.SetObjectName(m_Images[i], std::format("SwapchainImage[{}]", i).c_str());
			ctx.SetObjectName(m_ImageViews[i], std::format("SwapchainImageView[{}]", i).c_str());
		}

		TF_CORE_INFO("Swapchain: {0}x{1}, {2} images, {3}", m_Extent.width, m_Extent.height, m_Images.size(), vk::to_string(present));
		return {};
	}

	std::expected<void, std::string> VulkanSwapchain::Recreate()
	{
		(void)VulkanContext::Get().GetDevice().waitIdle();
		return Build();
	}

	void VulkanSwapchain::SetVSync(bool enabled)
	{
		if (m_VSync == enabled)
			return;

		m_VSync = enabled;
		(void)Recreate();
	}

	bool VulkanSwapchain::AcquireNext(vk::Semaphore imageAvailable, uint32_t& outImageIndex)
	{
		if (m_Extent.width == 0 || m_Extent.height == 0)
			return false;

		const auto result = VulkanContext::Get().GetDevice().acquireNextImage2KHR(
			{.swapchain = m_Swapchain, .timeout = UINT64_MAX, .semaphore = imageAvailable, .deviceMask = 1});

		if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR)
			return false;

		outImageIndex = result.value;
		return true;
	}

	bool VulkanSwapchain::Present(vk::Queue queue, vk::Semaphore renderFinished, uint32_t imageIndex)
	{
		const auto result = queue.presentKHR({.waitSemaphoreCount = 1,
			.pWaitSemaphores = &renderFinished,
			.swapchainCount = 1,
			.pSwapchains = &m_Swapchain,
			.pImageIndices = &imageIndex});

		return result == vk::Result::eSuccess;
	}

	void VulkanSwapchain::DestroyViews()
	{
		auto device = VulkanContext::Get().GetDevice();
		for (auto view : m_ImageViews)
			device.destroyImageView(view);

		m_ImageViews.clear();
	}

	void VulkanSwapchain::Shutdown()
	{
		auto& ctx = VulkanContext::Get();
		DestroyViews();

		if (m_Swapchain)
		{
			ctx.GetDevice().destroySwapchainKHR(m_Swapchain);
			m_Swapchain = nullptr;
		}

		if (m_Surface)
		{
			ctx.GetInstance().destroySurfaceKHR(m_Surface);
			m_Surface = nullptr;
		}
	}
}
