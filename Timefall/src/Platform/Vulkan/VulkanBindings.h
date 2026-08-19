#pragma once

#include "Timefall/RHI/Bindings.h"
#include "Timefall/Renderer/ShaderReflection.h"

#include <vulkan/vulkan.hpp>

namespace Timefall
{
	class VulkanBindings
	{
	public:
		static constexpr uint32_t SetCount = 3; // 0 per-frame, 1 per-pass, 2 bindless

		// Set 2's binding numbers, deliberately sparse: a variable-count binding must be the highest
		static constexpr uint32_t BindingSamplers = 0;
		static constexpr uint32_t BindingTextures = 15;

		static constexpr uint32_t kPushConstantBytes = 128;
		static constexpr vk::ShaderStageFlags kAllStages =
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;

		static void Init();
		static void Shutdown();

		static void BeginFrame(uint32_t slot);

		static vk::DescriptorSet GetBindlessSet();

		static vk::PipelineLayout GetGlobalLayout();

		static vk::PipelineLayout GetLayoutFor(const ShaderReflection& reflection, const std::string& debugName);

		static void BindGlobalSets(vk::CommandBuffer cmd, vk::PipelineLayout layout);
	};
}
