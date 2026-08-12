#include "tfpch.h"

#include "VulkanContext.h"

#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace Timefall
{
	static std::expected<PFN_vkGetInstanceProcAddr, std::string> LoadVulkanLibrary()
	{
		HMODULE vulkanLibrary = LoadLibraryA("vulkan-1.dll");
		if (!vulkanLibrary)
			return std::unexpected("vulkan-1.dll not found - no Vulkan driver installed?");

		auto gipa = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkanLibrary, "vkGetInstanceProcAddr"));
		if (!gipa)
			return std::unexpected("vkGetInstanceProcAddr missing from vulkan-1.dll");

		return gipa;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type,
		const vk::DebugUtilsMessengerCallbackDataEXT* data, void*)
	{
		using Severity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using Type = vk::DebugUtilsMessageTypeFlagBitsEXT;

		if (severity & Severity::eError)
			TF_CORE_ERROR("[Vulkan] {0}", data->pMessage);
		else if (severity & Severity::eWarning)
			TF_CORE_WARN("[Vulkan] {0}", data->pMessage);

		if (type & Type::ePerformance)
			TF_CORE_INFO("[Vulkan] {0}", data->pMessage);

		return vk::False;
	}

	VulkanContext& VulkanContext::Get()
	{
		static VulkanContext s_Instance;
		return s_Instance;
	}

	std::expected<void, std::string> Timefall::VulkanContext::Init(bool enableDebug)
	{
		m_DebugEnabled = enableDebug;

		auto gipa = LoadVulkanLibrary();
		if (!gipa)
			return std::unexpected(gipa.error());

		VULKAN_HPP_DEFAULT_DISPATCHER.init(*gipa);

		if (auto result = CreateInstance(); !result)
			return result;

		if (m_DebugEnabled)
		{
			if (auto result = CreateDebugMessenger(); !result)
				return result;
		}

		if (auto result = SelectPhysicalDevice(); !result)
			return result;

		if (auto result = CreateDevice(); !result)
			return result;

		if (auto result = CreateAllocator(); !result)
			return result;

		QueryLimits();
		return {};
	}

	std::expected<void, std::string> VulkanContext::CreateInstance()
	{
		const vk::ApplicationInfo app{.pApplicationName = "Timefall",
			.applicationVersion = 1,
			.pEngineName = "Timefall",
			.engineVersion = 1,
			.apiVersion = vk::ApiVersion14};

		std::vector<const char*> layers;
		std::vector<const char*> extensions;

		// VK_KHR_surface + the platform surface extension; required by the device's VK_KHR_swapchain.
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		if (!glfwExtensions)
			return std::unexpected("glfwGetRequiredInstanceExtensions failed - no Vulkan-capable window system");

		extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);

		// Required by getSurfaceCapabilities2KHR / getSurfaceFormats2KHR; GLFW never adds it.
		extensions.push_back(vk::KHRGetSurfaceCapabilities2ExtensionName);

		if (m_DebugEnabled)
		{
			layers.push_back("VK_LAYER_KHRONOS_validation");
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		const vk::ValidationFeatureEnableEXT enabled[]{
			vk::ValidationFeatureEnableEXT::eSynchronizationValidation, vk::ValidationFeatureEnableEXT::eBestPractices};

		const vk::ValidationFeaturesEXT validation{
			.enabledValidationFeatureCount = (uint32_t)std::size(enabled), .pEnabledValidationFeatures = enabled};

		auto instance = vk::createInstance({.pNext = m_DebugEnabled ? &validation : nullptr,
			.pApplicationInfo = &app,
			.enabledLayerCount = (uint32_t)layers.size(),
			.ppEnabledLayerNames = layers.data(),
			.enabledExtensionCount = (uint32_t)extensions.size(),
			.ppEnabledExtensionNames = extensions.data()});

		if (!instance)
			return std::unexpected(std::format("vkCreateInstance failed {0}", vk::to_string(instance.error())));

		m_Instance = *instance;
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Instance);

		TF_CORE_INFO("Vulkan instance created (1.4)");
		return {};
	}

	std::expected<void, std::string> VulkanContext::CreateDebugMessenger()
	{
		using Sev = vk::DebugUtilsMessageSeverityFlagBitsEXT;
		using Type = vk::DebugUtilsMessageTypeFlagBitsEXT;

		auto messenger = m_Instance.createDebugUtilsMessengerEXT({
			.messageSeverity = Sev::eError | Sev::eWarning,
			.messageType = Type::eGeneral | Type::eValidation | Type::ePerformance,
			.pfnUserCallback = DebugCallback,
		});

		if (!messenger)
			return std::unexpected(std::format("Debug messenger failed: {0}", vk::to_string(messenger.error())));

		m_Messenger = *messenger;
		return {};
	}

	void VulkanContext::SetObjectName(uint64_t handle, vk::ObjectType type, const char* name) const
	{
		if (!m_DebugEnabled || !m_Device)
			return;

		(void)m_Device.setDebugUtilsObjectNameEXT({.objectType = type, .objectHandle = handle, .pObjectName = name});
	}

	std::expected<void, std::string> VulkanContext::SelectPhysicalDevice()
	{
		auto devices = m_Instance.enumeratePhysicalDevices();
		if (!devices || devices->empty())
			return std::unexpected("No Vulkan physical devices found");

		for (const auto& candidate : *devices)
		{
			const auto& chain = candidate.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>();

			const auto& props = chain.get<vk::PhysicalDeviceProperties2>().properties;
			const auto& driver = chain.get<vk::PhysicalDeviceDriverProperties>();

			if (props.apiVersion < vk::ApiVersion14)
				continue;
			if (props.deviceType != vk::PhysicalDeviceType::eDiscreteGpu)
				continue;

			const auto& families = candidate.getQueueFamilyProperties2();
			for (uint32_t i = 0; i < families.size(); i++)
			{
				if (!(families[i].queueFamilyProperties.queueFlags & vk::QueueFlagBits::eGraphics))
					continue;

				m_PhysicalDevice = candidate;
				m_GraphicsQueueFamily = i;

				TF_CORE_INFO("Physical device: {0} ({1})", std::string_view(props.deviceName), std::string_view(driver.driverInfo));
				return {};
			}
		}

		return std::unexpected("No discrete GPU reporting Vulkan 1.4 with a graphics queue");
	}

	std::expected<void, std::string> VulkanContext::CreateDevice()
	{
		auto available = m_PhysicalDevice.enumerateDeviceExtensionProperties();
		if (available)
			m_Robustness2Avaiable = std::ranges::any_of(
				*available, [](const auto& e) { return std::string_view(e.extensionName) == vk::EXTRobustness2ExtensionName; });

		const auto& supported = m_PhysicalDevice.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features>();

		const auto& sBase = supported.get<vk::PhysicalDeviceFeatures2>().features;
		const auto& s12 = supported.get<vk::PhysicalDeviceVulkan12Features>();
		const auto& s13 = supported.get<vk::PhysicalDeviceVulkan13Features>();
		const auto& s14 = supported.get<vk::PhysicalDeviceVulkan14Features>();

		const std::pair<vk::Bool32, const char*> required[]{{s12.timelineSemaphore, "timelineSemaphore"},
			{s14.hostImageCopy, "hostImageCopy"}, {s14.maintenance5, "maintenance5"}, {s14.maintenance6, "maintenance6"},
			{s13.dynamicRendering, "dynamicRendering"}, {s13.synchronization2, "synchronization2"},
			{s12.descriptorIndexing, "descriptorIndexing"}, {s12.runtimeDescriptorArray, "runtimeDescriptorArray"},
			{s12.descriptorBindingPartiallyBound, "descriptorBindingPartiallyBound"},
			{s12.descriptorBindingVariableDescriptorCount, "descriptorBindingVariableDescriptorCount"},
			{s12.descriptorBindingSampledImageUpdateAfterBind, "descriptorBindingSampledImageUpdateAfterBind"},
			{s12.shaderSampledImageArrayNonUniformIndexing, "shaderSampledImageArrayNonUniformIndexing"},
			{sBase.samplerAnisotropy, "samplerAnisotropy"}};

		for (const auto& [ok, name] : required)
			if (!ok)
				return std::unexpected(std::format("Device lacks required feature {0}", name));

		vk::PhysicalDeviceVulkan14Features f14{};
		f14.hostImageCopy = vk::True; // vkCopyMemoryToImage - no staging buffer for textures
		f14.maintenance5 = vk::True; // bindIndexBuffer2, BufferUsageFlags2
		f14.maintenance6 = vk::True; // bindDescriptorSets2, pushConstants2, pushDescriptorSet2

		vk::PhysicalDeviceVulkan13Features f13{.pNext = &f14};
		f13.dynamicRendering = vk::True;
		f13.synchronization2 = vk::True;

		vk::PhysicalDeviceVulkan12Features f12{.pNext = &f13};
		f12.timelineSemaphore = vk::True; // the frame clock; there are no fences
		f12.descriptorIndexing = vk::True;
		f12.runtimeDescriptorArray = vk::True;
		f12.descriptorBindingPartiallyBound = vk::True;
		f12.descriptorBindingVariableDescriptorCount = vk::True;
		f12.descriptorBindingSampledImageUpdateAfterBind = vk::True;
		f12.shaderSampledImageArrayNonUniformIndexing = vk::True;

		vk::PhysicalDeviceVulkan11Features f11{.pNext = &f12};

		vk::PhysicalDeviceFeatures2 features2{.pNext = &f11};
		features2.features.samplerAnisotropy = vk::True;

		constexpr float priority = 1.0f;
		const vk::DeviceQueueCreateInfo queue{.queueFamilyIndex = m_GraphicsQueueFamily, .queueCount = 1, .pQueuePriorities = &priority};

		const char* deviceExtensions[]{vk::KHRSwapchainExtensionName};

		auto device = m_PhysicalDevice.createDevice({.pNext = &features2,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue,
			.enabledExtensionCount = (uint32_t)std::size(deviceExtensions),
			.ppEnabledExtensionNames = deviceExtensions,
			.pEnabledFeatures = nullptr});

		if (!device)
			return std::unexpected(std::format("vkCreateDevice failed {}", vk::to_string(device.error())));

		m_Device = *device;
		VULKAN_HPP_DEFAULT_DISPATCHER.init(m_Device);

		m_GraphicsQueue = m_Device.getQueue2({.queueFamilyIndex = m_GraphicsQueueFamily, .queueIndex = 0});

		if (!m_GraphicsQueue)
			return std::unexpected("vkGetDeviceQueue2 returned a null queue - DeviceQueueInfo2::flags mismatch");

		SetObjectName(m_Instance, "Instance");
		SetObjectName(m_PhysicalDevice, "PhysicalDevice");
		SetObjectName(m_Device, "Device");
		SetObjectName(m_GraphicsQueue, "GraphicsQueue");
		return {};
	}

	std::expected<void, std::string> VulkanContext::CreateAllocator()
	{
		VmaVulkanFunctions fns{};
		fns.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
		fns.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
		fns.vkGetPhysicalDeviceMemoryProperties2KHR = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceMemoryProperties2;

		const VmaAllocatorCreateInfo info{.physicalDevice = m_PhysicalDevice,
			.device = m_Device,
			.pVulkanFunctions = &fns,
			.instance = m_Instance,
			.vulkanApiVersion = VK_API_VERSION_1_4};

		if (vmaCreateAllocator(&info, &m_Allocator) != VK_SUCCESS)
			return std::unexpected("vmaCreateAllocator failed");

		TF_CORE_INFO("VMA allocator created");
		return {};
	}

	void VulkanContext::QueryLimits()
	{
		auto chain = m_PhysicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDescriptorIndexingProperties>();

		const auto& props = chain.get<vk::PhysicalDeviceProperties2>().properties;
		const auto& di = chain.get<vk::PhysicalDeviceDescriptorIndexingProperties>();

		const uint32_t deviceMax =
			std::min(di.maxDescriptorSetUpdateAfterBindSampledImages, di.maxPerStageDescriptorUpdateAfterBindSampledImages);

		m_Limits.MaxBindlessTextures = std::min(deviceMax, RHI::TF_BINDLESS_TEXTURE_BUDGET);
		m_Limits.MaxPushConstantSize = props.limits.maxPushConstantsSize;
		m_Limits.MinUniformBufferOffsetAlignment = props.limits.minUniformBufferOffsetAlignment;
		m_Limits.MinStorageBufferOffsetAlignment = props.limits.minStorageBufferOffsetAlignment;
		m_Limits.MaxSamplerAnisotropy = props.limits.maxSamplerAnisotropy;
		m_Limits.SupportsRobustness2 = m_Robustness2Avaiable;

		TF_CORE_INFO("Bindless textures: {0} (device reports {1}, budget {2})", m_Limits.MaxBindlessTextures, deviceMax,
			RHI::TF_BINDLESS_TEXTURE_BUDGET);
		TF_CORE_INFO("Push constant max: {0} bytes", m_Limits.MaxPushConstantSize);
		TF_CORE_INFO(
			"UBO/SSBO offset alignment: {0} / {1}", m_Limits.MinUniformBufferOffsetAlignment, m_Limits.MinStorageBufferOffsetAlignment);
		TF_CORE_INFO("Max sampler anisotropy: {0}", m_Limits.MaxSamplerAnisotropy);
		TF_CORE_INFO("Robustness2 available: {0}", m_Limits.SupportsRobustness2 ? "yes" : "no");
	}

	void VulkanContext::Shutdown()
	{
		if (m_Device)
			(void)m_Device.waitIdle();

		if (m_Allocator)
		{
			vmaDestroyAllocator(m_Allocator);
			m_Allocator = nullptr;
		}

		if (m_Device)
		{
			m_Device.destroy();
			m_Device = nullptr;
		}

		if (m_Messenger)
		{
			m_Instance.destroyDebugUtilsMessengerEXT(m_Messenger);
			m_Messenger = nullptr;
		}

		if (m_Instance)
		{
			m_Instance.destroy();
			m_Instance = nullptr;
		}
	}
}
