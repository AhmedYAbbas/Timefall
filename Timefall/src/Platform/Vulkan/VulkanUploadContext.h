#pragma once

#include <vulkan/vulkan.hpp>

#include <functional>

namespace Timefall
{
	class VulkanUploadContext
	{
	public:
		static void Init(uint64_t stagingBytes = 32 * 1024 * 1024);
		static void Shutdown();

		static void Begin();
		static void End();

		static bool UploadBuffer(vk::Buffer dst, uint64_t dstOffset, const void* data, uint64_t size);
		static void Record(const std::function<void(vk::CommandBuffer)>& fn);

		static void Flush();
	};
}
