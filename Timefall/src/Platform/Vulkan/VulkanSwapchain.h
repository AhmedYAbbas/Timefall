#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

struct GLFWwindow;

namespace Timefall
{
	class VulkanSwapchain
	{
	public:
		std::expected<void, std::string> Init(GLFWwindow* window);
		void Shutdown();

		std::expected<void, std::string> Recreate();
		void SetVSync(bool enabled);

		// false means out-of-date: rebuild and skip the frame
		bool AcquireNext(vk::Semaphore imageAvailable, uint32_t& outImageIndex);
		bool Present(vk::Queue queue, vk::Semaphore renderFinished, uint32_t imageIndex);

		vk::Format GetFormat() const { return m_Format; }
		vk::Extent2D GetExtent() const { return m_Extent; }
		uint32_t GetImageCount() const { return (uint32_t)m_Images.size(); }
		uint32_t GetMinImageCount() const { return m_MinImageCount; }
		vk::Image GetImage(uint32_t i) const { return m_Images[i]; }
		vk::ImageView GetImageView(uint32_t i) const { return m_ImageViews[i]; }

		// Live layout of each image, owned here because a rebuild resets every image to Undefined.
		vk::ImageLayout& GetImageLayout(uint32_t i) { return m_ImageLayouts[i]; }

	private:
		std::expected<void, std::string> Build();
		void DestroyViews();

	private:
		GLFWwindow* m_Window = nullptr;
		vk::SurfaceKHR m_Surface;
		vk::SwapchainKHR m_Swapchain;
		std::vector<vk::Image> m_Images;
		std::vector<vk::ImageView> m_ImageViews;
		std::vector<vk::ImageLayout> m_ImageLayouts;
		vk::Format m_Format = vk::Format::eUndefined;
		vk::Extent2D m_Extent{};
		uint32_t m_MinImageCount = 0;
		bool m_VSync = true;
	};
}
