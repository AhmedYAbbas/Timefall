#include "tfpch.h"
#include "VulkanBindings.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanBindlessTable.h"

#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Texture.h"

namespace Timefall
{
	namespace
	{
		vk::DescriptorPool s_Pool;
		std::array<vk::DescriptorSetLayout, VulkanBindings::SetCount> s_SetLayouts;
		std::array<vk::DescriptorSet, VulkanBindings::SetCount> s_Sets{};

		vk::PipelineLayout s_GlobalLayout;

		Ref<RHI::GpuBuffer> s_FrameUniforms;
		Ref<RHI::Texture> s_WhiteTexture;
		uint64_t s_FrameUniformStride = 0;
		uint32_t s_Slot = 0;
		bool s_Ready = false;

		bool CreatePool(uint32_t bindlessCapacity)
		{
			const vk::DescriptorPoolSize sizes[]{
				{vk::DescriptorType::eUniformBufferDynamic, 1}, {vk::DescriptorType::eSampler, (uint32_t)RHI::SamplerSlot::Count}, {vk::DescriptorType::eSampledImage, bindlessCapacity}};

			auto pool = VulkanContext::Get().GetDevice().createDescriptorPool({
				.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind,
				.maxSets = VulkanBindings::SetCount, .poolSizeCount = (uint32_t)std::size(sizes),
				.pPoolSizes = sizes});

			if (!pool)
			{
				TF_CORE_ERROR("createDescriptorPool failed: {0}", vk::to_string(pool.error()));
				return false;
			}

			s_Pool = *pool;
			VulkanContext::Get().SetObjectName(s_Pool, "Bindings:DescriptorPool");
			return true;
		}

		vk::DescriptorSetLayout CreateSetLayout(std::span<const vk::DescriptorSetLayoutBinding> bindings,
			std::span<const vk::DescriptorBindingFlags> flags, bool updateAfterBind, const char* name)
		{
			const vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
				.bindingCount = (uint32_t)flags.size(), .pBindingFlags = flags.data()};

			auto layout = VulkanContext::Get().GetDevice().createDescriptorSetLayout({.pNext = &flagsInfo,
				.flags = updateAfterBind ? vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool : vk::DescriptorSetLayoutCreateFlagBits{},
				.bindingCount = (uint32_t)bindings.size(),
				.pBindings = bindings.data()});

			if (!layout)
			{
				TF_CORE_ERROR("createDescriptorSetLayout failed for '{0}: {1}", name, vk::to_string(layout.error()));
				return nullptr;
			}

			VulkanContext::Get().SetObjectName(*layout, name);
			return *layout;
		}

		bool CreateSetLayouts(uint32_t bindlessCapacity)
		{
			const vk::DescriptorSetLayoutBinding frameBindings[]{{.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBufferDynamic,
				.descriptorCount = 1,
				.stageFlags = VulkanBindings::kAllStages}};
			const vk::DescriptorBindingFlags frameFlags[]{{}};

			s_SetLayouts[0] = CreateSetLayout(frameBindings, frameFlags, false, "Bindings:Set0_PerFrame");
			s_SetLayouts[1] = CreateSetLayout({}, {}, false, "Bindings:Set1_PerPass");

			const vk::DescriptorSetLayoutBinding bindlessBindings[]{
				{.binding = VulkanBindings::BindingSamplers,
				.descriptorType = vk::DescriptorType::eSampler,
				.descriptorCount = (uint32_t)RHI::SamplerSlot::Count,
				.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingTextures,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.descriptorCount = bindlessCapacity,
				.stageFlags = VulkanBindings::kAllStages}
			};

			const vk::DescriptorBindingFlags bindlessFlags[]{{}, vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::eVariableDescriptorCount};

			s_SetLayouts[2] = CreateSetLayout(bindlessBindings, bindlessFlags, true, "Bindings:Set2_Bindless");

			return s_SetLayouts[0] && s_SetLayouts[1] && s_SetLayouts[2];
		}

		bool AllocateSets(uint32_t bindlessCapacity)
		{
			auto device = VulkanContext::Get().GetDevice();

			const vk::DescriptorSetLayout plain[]{s_SetLayouts[0], s_SetLayouts[1]};
			auto plainSets =
				device.allocateDescriptorSets({.descriptorPool = s_Pool, .descriptorSetCount = (uint32_t)std::size(plain), .pSetLayouts = plain});
			if (!plainSets)
			{
				TF_CORE_ERROR("allocateDescriptorSets failed for sets 0-1: {0}", vk::to_string(plainSets.error()));
				return false;
			}

			s_Sets[0] = (*plainSets)[0];
			s_Sets[1] = (*plainSets)[1];

			const vk::DescriptorSetVariableDescriptorCountAllocateInfo variableCount{.descriptorSetCount = 1, .pDescriptorCounts = &bindlessCapacity};
			auto bindlessSet = device.allocateDescriptorSets(
				{.pNext = &variableCount, .descriptorPool = s_Pool, .descriptorSetCount = 1, .pSetLayouts = &s_SetLayouts[2]});
			if (!bindlessSet)
			{
				TF_CORE_ERROR("allocateDescriptorSets failed for set 2: {0}", vk::to_string(bindlessSet.error()));
				return false;
			}

			s_Sets[2] = (*bindlessSet)[0];

			auto& ctx = VulkanContext::Get();
			ctx.SetObjectName(s_Sets[0], "Bindings:Set0");
			ctx.SetObjectName(s_Sets[1], "Bindings:Set1");
			ctx.SetObjectName(s_Sets[2], "Bindings:Set2");
			return true;
		}

		bool CreateFrameUniforms()
		{
			s_FrameUniformStride = RHI::FrameUniformSlotBytes;
			const uint64_t alignment = VulkanContext::Get().GetLimits().MinUniformBufferOffsetAlignment;
			if (alignment > 0)
				s_FrameUniformStride = (RHI::FrameUniformSlotBytes + alignment - 1) & ~(alignment - 1);

			s_FrameUniforms = RHI::GpuBuffer::Create({.Size = s_FrameUniformStride * RHI::FRAMES_IN_FLIGHT,
				.Usage = RHI::BufferUsage::Uniform,
				.Memory = RHI::MemoryType::HostWrite,
				.DebugName = "FrameUniforms"});

			if (!s_FrameUniforms || !s_FrameUniforms->IsValid())
			{
				TF_CORE_ERROR("FrameUniforms buffer failed to allocate");
				return false;
			}

			TF_CORE_INFO("Frame uniforms: {0} bytes per slot, {1} byte stride (alignmnet {2})", RHI::FrameUniformSlotBytes,
				s_FrameUniformStride, alignment);
			return true;
		}

		bool CreateWhiteTexture()
		{
			constexpr uint32_t white = 0xFFFFFFFF;

			s_WhiteTexture = RHI::Texture::CreateWithData(
				{.Width = 1, .Height = 1, .PixelFormat = RHI::Format::RGBA8Unorm, .MipLevels = 1, .SRGBView = false, .DebugName = "BindlessWhite"}, &white, sizeof(white));

			if (!s_WhiteTexture || !s_WhiteTexture->IsValid())
			{
				TF_CORE_ERROR("The bindless white texture failed to create");
				return false;
			}

			return true;
		}

		void WriteInitialDescriptors()
		{
			const vk::DescriptorBufferInfo frameInfo{.buffer = (VkBuffer)s_FrameUniforms->GetNativeHandle(), .offset = 0, .range = RHI::FrameUniformSlotBytes};

			std::array<vk::DescriptorImageInfo, (size_t)RHI::SamplerSlot::Count> samplerInfos{};
			for (uint32_t i = 0; i < (uint32_t)RHI::SamplerSlot::Count; i++)
				samplerInfos[i].sampler = VulkanSamplerCache::Get((RHI::SamplerSlot)i);

			const vk::WriteDescriptorSet writes[]{{.dstSet = s_Sets[0], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBufferDynamic, .pBufferInfo = &frameInfo},
				{.dstSet = s_Sets[2],
					.dstBinding = VulkanBindings::BindingSamplers,
					.dstArrayElement = 0,
					.descriptorCount = (uint32_t)samplerInfos.size(),
					.descriptorType = vk::DescriptorType::eSampler,
					.pImageInfo = samplerInfos.data()}};

			VulkanContext::Get().GetDevice().updateDescriptorSets(writes, {});
		}

		bool CreateGlobalLayout()
		{
			const vk::PushConstantRange pushRange{.stageFlags = VulkanBindings::kAllStages, .offset = 0, .size = VulkanBindings::kPushConstantBytes};

			auto layout = VulkanContext::Get().GetDevice().createPipelineLayout(
				{.setLayoutCount = VulkanBindings::SetCount, .pSetLayouts = s_SetLayouts.data(), .pushConstantRangeCount = 1, .pPushConstantRanges = &pushRange});

			if (!layout)
			{
				TF_CORE_ERROR("createPipelineLayout failed for the global prefix: {0}", vk::to_string(layout.error()));
				return false;
			}

			s_GlobalLayout = *layout;
			VulkanContext::Get().SetObjectName(s_GlobalLayout, "Bindings:GlobalLayout");
			return true;
		}
	}

	void VulkanBindings::Init()
	{
		TF_PROFILE_FUNCTION();

		const uint32_t capacity = VulkanContext::Get().GetLimits().MaxBindlessTextures;
		if (capacity == 0)
		{
			TF_CORE_ERROR("MaxBindlessTextures is 0 - the binding model cannot initialize");
			return;
		}

		if (!CreatePool(capacity) || !CreateSetLayouts(capacity) || !AllocateSets(capacity) || !CreateFrameUniforms()
			|| !CreateGlobalLayout() || !CreateWhiteTexture())
		{
			Shutdown();
			return;
		}

		WriteInitialDescriptors();
		VulkanBindlessTable::Init(s_Sets[2], capacity, (VkImageView)s_WhiteTexture->GetNativeView());
		s_Ready = true;

		TF_CORE_INFO(
			"Binding model ready: {0} sets, {1} bindless slots, {2} byte push range", (uint32_t)SetCount, capacity, kPushConstantBytes);
	}

	void VulkanBindings::Shutdown()
	{
		s_Ready = false;

		auto device = VulkanContext::Get().GetDevice();

		VulkanBindlessTable::Shutdown();
		s_WhiteTexture.reset();
		s_FrameUniforms.reset();

		if (s_GlobalLayout)
			device.destroyPipelineLayout(s_GlobalLayout);
		s_GlobalLayout = nullptr;

		if (s_Pool)
			device.destroyDescriptorPool(s_Pool);
		s_Pool = nullptr;
		s_Sets.fill(nullptr);

		for (auto& layout : s_SetLayouts)
		{
			if (layout)
				device.destroyDescriptorSetLayout(layout);
			layout = nullptr;
		}

		s_FrameUniformStride = 0;
		s_Slot = 0;
	}

	void VulkanBindings::BeginFrame(uint32_t slot)
	{
		s_Slot = slot % RHI::FRAMES_IN_FLIGHT;
	}

	vk::DescriptorSet VulkanBindings::GetBindlessSet()
	{
		return s_Sets[2];
	}

	vk::PipelineLayout VulkanBindings::GetGlobalLayout()
	{
		return s_GlobalLayout;
	}

	void VulkanBindings::BindGlobalSets(vk::CommandBuffer cmd, vk::PipelineLayout layout)
	{
		if (!s_Ready || !layout)
			return;

		const uint32_t dynamicOffset = (uint32_t)(s_Slot * s_FrameUniformStride);

		cmd.bindDescriptorSets2({.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			.layout = layout,
			.firstSet = 0,
			.descriptorSetCount = SetCount,
			.pDescriptorSets = s_Sets.data(), .dynamicOffsetCount = 1, .pDynamicOffsets = &dynamicOffset});
	}

	bool RHI::Bindings::WriteFrameUniforms(const void* data, uint64_t size)
	{
		if (!s_Ready || !data)
			return false;

		if (size > RHI::FrameUniformSlotBytes)
		{
			TF_CORE_ERROR("WriteFrameUniforms got {0} bytes for a {1} byte slot", size, RHI::FrameUniformSlotBytes);
			return false;
		}

		return s_FrameUniforms->Write(data, size, s_Slot * s_FrameUniformStride);
	}

	uint32_t RHI::Bindings::GetBindlessCapacity()
	{
		return VulkanBindlessTable::GetCapacity();
	}

	uint32_t RHI::Bindings::GetBindlessUsed()
	{
		return VulkanBindlessTable::GetUsed();
	}

	uint32_t RHI::Bindings::GetWhiteTextureIndex()
	{
		return VulkanBindlessTable::WhiteIndex;
	}
}
