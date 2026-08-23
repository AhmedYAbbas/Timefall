#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>

namespace Timefall
{
	class VulkanBindlessTable
	{
	public:
		static constexpr uint32_t WhiteIndex = 0;
		static constexpr uint32_t WhiteCubeIndex = 0;
		static constexpr uint32_t InvalidIndex = UINT32_MAX;

		static void Init(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteView);
		static void InitCubes(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteCubeView);
		static void Shutdown();

		static uint32_t Acquire(vk::ImageView view);
		static void Release(uint32_t index);

		static uint32_t AcquireCube(vk::ImageView view);
		static void ReleaseCube(uint32_t index);

		static uint32_t GetCapacity();
		static uint32_t GetUsed();
		static uint32_t GetCubeUsed();
	};
}
