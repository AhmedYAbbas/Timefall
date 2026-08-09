#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <expected>
#include <string>

namespace Timefall
{
	class TF_API VulkanContext
	{
	public:
		static VulkanContext& Get();

		std::expected<void, std::string> Init(bool enableDebug);
		void Shutdown();

		vk::Instance GetInstance() const { return m_Instance; }
		vk::PhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
		vk::Device GetDevice() const { return m_Device; }
		vk::Queue GetGraphicsQueue() const { return m_GraphicsQueue; }
		uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
		VmaAllocator GetAllocator() const { return m_Allocator; }

		const RHI::Limits& GetLimits() const { return m_Limits; }
		bool IsDebugEnabled() const { return m_DebugEnabled; }

		void SetObjectName(uint64_t handle, vk::ObjectType type, const char* name) const;

	private:
		std::expected<void, std::string> CreateInstance();
		std::expected<void, std::string> CreateDebugMessenger();
		std::expected<void, std::string> SelectPhysicalDevice();
		std::expected<void, std::string> CreateDevice();
		std::expected<void, std::string> CreateAllocator();
		void QueryLimits();

	private:
		bool m_DebugEnabled = true;
		bool m_Robustness2Avaiable = false;

		vk::Instance m_Instance;
		vk::DebugUtilsMessengerEXT m_Messenger;
		vk::PhysicalDevice m_PhysicalDevice;
		vk::Device m_Device;
		vk::Queue m_GraphicsQueue;
		uint32_t m_GraphicsQueueFamily = UINT32_MAX;
		VmaAllocator m_Allocator = nullptr;
		RHI::Limits m_Limits;
	};
}
