#pragma once

#include "Timefall/RHI/Bindings.h"

#include <vulkan/vulkan.hpp>

namespace Timefall
{
	class VulkanSamplerCache
	{
	public:
		struct SamplerDesc
		{
			vk::Filter MinMagFilter = vk::Filter::eLinear;
			vk::SamplerMipmapMode MipMode = vk::SamplerMipmapMode::eLinear;
			vk::SamplerAddressMode AddressMode = vk::SamplerAddressMode::eRepeat;
			bool Anisotropic = false;
			bool CompareDepth = false;

			bool operator==(const SamplerDesc&) const = default;
		};

		static void Init();
		static void Shutdown();

		static vk::Sampler Get(const SamplerDesc& desc);
		static vk::Sampler Get(RHI::SamplerSlot slot);
	};
}
