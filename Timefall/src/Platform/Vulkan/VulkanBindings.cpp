#include "tfpch.h"
#include "VulkanBindings.h"

#include "Platform/Vulkan/VulkanContext.h"
#include "Platform/Vulkan/VulkanSamplerCache.h"
#include "Platform/Vulkan/VulkanBindlessTable.h"

#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Texture.h"

#include "Timefall/Renderer/ShaderReflection.h"

namespace Timefall
{
	namespace
	{
		vk::DescriptorPool s_Pool;
		std::array<vk::DescriptorSetLayout, VulkanBindings::SetCount> s_SetLayouts;
		std::array<vk::DescriptorSet, VulkanBindings::SetCount> s_Sets{};

		std::unordered_map<uint64_t, vk::PipelineLayout> s_LayoutCache;
		std::vector<vk::DescriptorSetLayout> s_PrivateSetLayouts;

		vk::PipelineLayout s_GlobalLayout;

		Ref<RHI::GpuBuffer> s_FrameUniforms;
		Ref<RHI::Texture> s_WhiteTexture;
		Ref<RHI::Texture> s_WhiteCubeTexture;
		Ref<RHI::Texture> s_WhiteArrayTexture;
		Ref<RHI::Texture> s_WhiteCubeArrayTexture;
		uint64_t s_FrameUniformStride = 0;
		uint32_t s_Slot = 0;
		bool s_Ready = false;

		Ref<RHI::GpuBuffer> s_PassUniforms;
		uint64_t s_PassUniformStride = 0;
		uint32_t s_PassSlice = 0;
		bool s_PassSliceExhausted = false;

		bool CreatePool(uint32_t bindlessCapacity)
		{
			const vk::DescriptorPoolSize sizes[]{{vk::DescriptorType::eUniformBufferDynamic, 2},
				{vk::DescriptorType::eSampler, (uint32_t)RHI::SamplerSlot::Count},
				{vk::DescriptorType::eSampledImage,
					bindlessCapacity + VulkanBindings::kTextureCubeCapacity + VulkanBindings::kTextureArrayCapacity
						+ VulkanBindings::kTextureCubeArrayCapacity},
				{vk::DescriptorType::eSampler, (uint32_t)RHI::ComparisonSamplerSlot::Count}};

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
				TF_CORE_ERROR("createDescriptorSetLayout failed for '{0}': {1}", name, vk::to_string(layout.error()));
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

			const vk::DescriptorSetLayoutBinding passBindings[]{{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBufferDynamic, .descriptorCount = 1, .stageFlags = VulkanBindings::kAllStages}};
			const vk::DescriptorBindingFlags passFlags[]{{}};

			s_SetLayouts[1] = CreateSetLayout(passBindings, passFlags, false, "Bindings:Set1_PerPass");

			const vk::DescriptorSetLayoutBinding bindlessBindings[]{
				{.binding = VulkanBindings::BindingSamplers,
				.descriptorType = vk::DescriptorType::eSampler,
				.descriptorCount = (uint32_t)RHI::SamplerSlot::Count,
				.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingTextureCubes,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.descriptorCount = VulkanBindings::kTextureCubeCapacity,
				.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingTextureArrays,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.descriptorCount = VulkanBindings::kTextureArrayCapacity,
				.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingTextureCubeArrays,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.descriptorCount = VulkanBindings::kTextureCubeArrayCapacity,
				.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingComparisonSamplers,
					.descriptorType = vk::DescriptorType::eSampler,
					.descriptorCount = (uint32_t)RHI::ComparisonSamplerSlot::Count,
					.stageFlags = VulkanBindings::kAllStages},

				{.binding = VulkanBindings::BindingTextures,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.descriptorCount = bindlessCapacity,
				.stageFlags = VulkanBindings::kAllStages}
			};

			const vk::DescriptorBindingFlags bindlessFlags[]{{},
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind,
				{},
				vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind
					| vk::DescriptorBindingFlagBits::eVariableDescriptorCount};

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

			TF_CORE_INFO("Frame uniforms: {0} bytes per slot, {1} byte stride (alignment {2})", RHI::FrameUniformSlotBytes,
				s_FrameUniformStride, alignment);
			return true;
		}

		bool CreatePassUniforms()
		{
			s_PassUniformStride = RHI::PassUniformSlotBytes;
			const uint64_t alignment = VulkanContext::Get().GetLimits().MinUniformBufferOffsetAlignment;
			if (alignment > 0)
				s_PassUniformStride = (RHI::PassUniformSlotBytes + alignment - 1) & ~(alignment - 1);

			s_PassUniforms = RHI::GpuBuffer::Create({.Size = s_PassUniformStride * RHI::PassUniformSlices * RHI::FRAMES_IN_FLIGHT,
				.Usage = RHI::BufferUsage::Uniform,
				.Memory = RHI::MemoryType::HostWrite,
				.DebugName = "PassUniforms"});

			if (!s_PassUniforms || !s_PassUniforms->IsValid())
			{
				TF_CORE_ERROR("PassUniforms buffer failed to allocate");
				return false;
			}

			TF_CORE_INFO("Pass uniforms: {0} bytes per slot, {1} byte stride, {2} slices x {3} frames", RHI::PassUniformSlotBytes, s_PassUniformStride, RHI::PassUniformSlices, (uint32_t)RHI::FRAMES_IN_FLIGHT);
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

		bool CreateWhiteCubeTexture()
		{
			constexpr uint32_t white[6]{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};

			s_WhiteCubeTexture = RHI::Texture::CreateWithData({.Width = 1,
				.Height = 1,
				.PixelFormat = RHI::Format::RGBA8Unorm,
				.MipLevels = 1,
				.Dim = RHI::Dimension::Cube,
				.ArrayLayers = 6,
				.DebugName = "BindlessWhiteCube"},
				white, sizeof(white));

			if (!s_WhiteCubeTexture || !s_WhiteCubeTexture->IsValid())
			{
				TF_CORE_ERROR("The bindless white cube failed to create");
				return false;
			}

			return true;
		}

		bool CreateWhiteArrayTexture()
		{
			const uint32_t white = 0xFFFFFFFF;

			s_WhiteArrayTexture = RHI::Texture::CreateWithData({.Width = 1,
																   .Height = 1,
																   .PixelFormat = RHI::Format::RGBA8Unorm,
																   .MipLevels = 1,
																   .Dim = RHI::Dimension::Tex2DArray,
																   .ArrayLayers = 1,
																   .DebugName = "BindlessWhiteArray"},
				&white, sizeof(white));

			if (!s_WhiteArrayTexture || !s_WhiteArrayTexture->IsValid())
			{
				TF_CORE_ERROR("The bindless white array texture failed to create");
				return false;
			}

			return true;
		}

		bool CreateWhiteCubeArrayTexture()
		{
			constexpr uint32_t white[6]{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
			s_WhiteCubeArrayTexture = RHI::Texture::CreateWithData({.Width = 1,
																	   .Height = 1,
																	   .PixelFormat = RHI::Format::RGBA8Unorm,
																	   .MipLevels = 1,
																	   .Dim = RHI::Dimension::CubeArray,
																	   .ArrayLayers = 6,
																	   .DebugName = "BindlessWhiteCubeArray"},
				white, sizeof(white));

			if (!s_WhiteCubeArrayTexture || !s_WhiteCubeArrayTexture->IsValid())
			{
				TF_CORE_ERROR("The bindless white cube array texture failed to create");
				return false;
			}

			return true;
		}

		void WriteInitialDescriptors()
		{
			const vk::DescriptorBufferInfo frameInfo{.buffer = (VkBuffer)s_FrameUniforms->GetNativeHandle(), .offset = 0, .range = RHI::FrameUniformSlotBytes};
			const vk::DescriptorBufferInfo passInfo{.buffer = (VkBuffer)s_PassUniforms->GetNativeHandle(), .offset = 0, .range = RHI::PassUniformSlotBytes};

			std::array<vk::DescriptorImageInfo, (size_t)RHI::SamplerSlot::Count> samplerInfos{};
			for (uint32_t i = 0; i < (uint32_t)RHI::SamplerSlot::Count; i++)
				samplerInfos[i].sampler = VulkanSamplerCache::Get((RHI::SamplerSlot)i);

			std::array < vk::DescriptorImageInfo, (size_t)RHI::ComparisonSamplerSlot::Count> cmpSamplerInfos{};
			for (uint32_t i = 0; i < (uint32_t)RHI::ComparisonSamplerSlot::Count; i++)
				cmpSamplerInfos[i].sampler = VulkanSamplerCache::Get((RHI::ComparisonSamplerSlot)i);

			const vk::WriteDescriptorSet writes[]{{.dstSet = s_Sets[0], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBufferDynamic, .pBufferInfo = &frameInfo},
				{.dstSet = s_Sets[1], .dstBinding = 0, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = vk::DescriptorType::eUniformBufferDynamic, .pBufferInfo = &passInfo},
				{.dstSet = s_Sets[2],
					.dstBinding = VulkanBindings::BindingSamplers,
					.dstArrayElement = 0,
					.descriptorCount = (uint32_t)samplerInfos.size(),
					.descriptorType = vk::DescriptorType::eSampler,
					.pImageInfo = samplerInfos.data()},
				{.dstSet = s_Sets[2],
					.dstBinding = VulkanBindings::BindingComparisonSamplers,
					.dstArrayElement = 0,
					.descriptorCount = (uint32_t)cmpSamplerInfos.size(),
					.descriptorType = vk::DescriptorType::eSampler,
					.pImageInfo = cmpSamplerInfos.data()}};

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

		struct CanonicalBinding
		{
			uint32_t Set;
			uint32_t Binding;
			ShaderBindingType Type;
			uint32_t Count;
		};

		constexpr CanonicalBinding kCanonical[]{{0, 0, ShaderBindingType::UniformBuffer, 1},
			{1, 0, ShaderBindingType::UniformBuffer, 1},
			{2, VulkanBindings::BindingSamplers, ShaderBindingType::Sampler, (uint32_t)RHI::SamplerSlot::Count},
			{2, VulkanBindings::BindingTextureCubes, ShaderBindingType::SampledImage, VulkanBindings::kTextureCubeCapacity},
			{2, VulkanBindings::BindingTextureArrays, ShaderBindingType::SampledImage, VulkanBindings::kTextureArrayCapacity},
			{2, VulkanBindings::BindingTextureCubeArrays, ShaderBindingType::SampledImage, VulkanBindings::kTextureCubeArrayCapacity},
			{2, VulkanBindings::BindingComparisonSamplers, ShaderBindingType::Sampler, (uint32_t)RHI::ComparisonSamplerSlot::Count},
			{2, VulkanBindings::BindingTextures, ShaderBindingType::SampledImage, 0}};

		const char* BindingTypeName(ShaderBindingType type)
		{
			switch (type)
			{
				case ShaderBindingType::UniformBuffer: return "UniformBuffer";
				case ShaderBindingType::StorageBuffer: return "StorageBuffer";
				case ShaderBindingType::SampledImage:  return "SampledImage";
				default:							   return "Sampler";
			}
		}

		bool ValidateAndSplit(const ShaderReflection& reflection, const std::string& name, std::vector<ShaderBinding>& privateBindings)
		{
			bool ok = true;

			if (reflection.PushConstantSize > VulkanBindings::kPushConstantBytes)
			{
				TF_CORE_ERROR("'{0}': push constant block is {1} bytes; the shared range is {2}", name, reflection.PushConstantSize,
					VulkanBindings::kPushConstantBytes);
				ok = false;
			}

			for (const auto& binding : reflection.Bindings)
			{
				TF_CORE_INFO(
					"'{0}': set {1} binding {2} {3} count {4}", name, binding.Set, binding.Binding, BindingTypeName(binding.Type), binding.Count);

				if (binding.Set >= VulkanBindings::SetCount)
				{
					privateBindings.push_back(binding);
					continue;
				}

				const auto match = std::ranges::find_if(
					kCanonical, [&binding](const CanonicalBinding& c) { return c.Set == binding.Set && c.Binding == binding.Binding; });

				if (match == std::end(kCanonical))
				{
					TF_CORE_ERROR(
						"'{0}': set {1} binding {2} is not part of the canonical layout - private resources go in set {3} or above", name,
						binding.Set, binding.Binding, VulkanBindings::SetCount);
					ok = false;
					continue;
				}

				if (match->Type != binding.Type || match->Count != binding.Count)
				{
					TF_CORE_ERROR("'{0}': set {1} binding {2} is declared {3}[{4}] but the canonical layout says {5}[{6}]", name,
						binding.Set, binding.Binding, BindingTypeName(binding.Type), binding.Count, BindingTypeName(match->Type),
						match->Count);
					ok = false;
				}
			}

			return ok;
		}

		uint64_t HashPrivateBindings(const std::vector<ShaderBinding>& bindings)
		{
			uint64_t hash = 14695981039346656037ull;
			const auto mix = [&hash](uint32_t value) {
				for (uint32_t byte = 0; byte < 4; byte++)
				{
					hash ^= (value >> (byte * 8)) & 0xFF;
					hash *= 1099511628211ull;
				}
			};

			for (const auto& binding : bindings)
			{
				mix(binding.Set);
				mix(binding.Binding);
				mix((uint32_t)binding.Type);
				mix(binding.Count);
			}

			return hash;
		}

		vk::DescriptorType ToVkDescriptorType(ShaderBindingType type)
		{
			switch (type)
			{
				case ShaderBindingType::UniformBuffer: return vk::DescriptorType::eUniformBuffer;
				case ShaderBindingType::StorageBuffer: return vk::DescriptorType::eStorageBuffer;
				case ShaderBindingType::SampledImage:  return vk::DescriptorType::eSampledImage;
				default:							   return vk::DescriptorType::eSampler;
			}
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
			|| !CreatePassUniforms() || !CreateGlobalLayout() || !CreateWhiteTexture() || !CreateWhiteCubeTexture() || !CreateWhiteArrayTexture() || !CreateWhiteCubeArrayTexture())
		{
			Shutdown();
			return;
		}

		WriteInitialDescriptors();
		VulkanBindlessTable::Init(s_Sets[2], capacity, (VkImageView)s_WhiteTexture->GetNativeView());
		VulkanBindlessTable::InitCubes(s_Sets[2], kTextureCubeCapacity, (VkImageView)s_WhiteCubeTexture->GetNativeView());
		VulkanBindlessTable::InitArrays(s_Sets[2], kTextureArrayCapacity, (VkImageView)s_WhiteArrayTexture->GetNativeView());
		VulkanBindlessTable::InitCubeArrays(s_Sets[2], kTextureCubeArrayCapacity, (VkImageView)s_WhiteCubeArrayTexture->GetNativeView());
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
		s_WhiteCubeTexture.reset();
		s_WhiteArrayTexture.reset();
		s_WhiteCubeArrayTexture.reset();
		s_PassUniforms.reset();
		s_FrameUniforms.reset();

		for (auto& [key, layout] : s_LayoutCache)
			device.destroyPipelineLayout(layout);
		s_LayoutCache.clear();

		for (auto& layout : s_PrivateSetLayouts)
			device.destroyDescriptorSetLayout(layout);
		s_PrivateSetLayouts.clear();

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
		s_PassUniformStride = 0;
		s_PassSlice = 0;
		s_PassSliceExhausted = false;
		s_Slot = 0;
	}

	void VulkanBindings::BeginFrame(uint32_t slot)
	{
		s_Slot = slot % RHI::FRAMES_IN_FLIGHT;
		s_PassSlice = 0;
		s_PassSliceExhausted = false;
	}

	vk::DescriptorSet VulkanBindings::GetBindlessSet()
	{
		return s_Sets[2];
	}

	vk::PipelineLayout VulkanBindings::GetGlobalLayout()
	{
		return s_GlobalLayout;
	}

	vk::PipelineLayout VulkanBindings::GetLayoutFor(const ShaderReflection& reflection, const std::string& debugName)
	{
		if (!s_Ready)
		{
			TF_CORE_ERROR("GetLayoutFor before VulkanBindings::Init");
			return nullptr;
		}

		std::vector<ShaderBinding> privateBindings;
		if (!ValidateAndSplit(reflection, debugName, privateBindings))
			return nullptr;

		if (privateBindings.empty())
			return s_GlobalLayout;

		const uint64_t key = HashPrivateBindings(privateBindings);
		if (auto it = s_LayoutCache.find(key); it != s_LayoutCache.end())
			return it->second;

		uint32_t highestSet = SetCount - 1;
		for (const auto& binding : privateBindings)
			highestSet = std::max(highestSet, binding.Set);

		std::vector<vk::DescriptorSetLayout> layouts{s_SetLayouts.begin(), s_SetLayouts.end()};

		for (uint32_t set = SetCount; set <= highestSet; set++)
		{
			std::vector<vk::DescriptorSetLayoutBinding> bindings;
			for (const auto& binding : privateBindings)
			{
				if (binding.Set != set)
					continue;

				bindings.push_back({.binding = binding.Binding, .descriptorType = ToVkDescriptorType(binding.Type), .descriptorCount = binding.Count, .stageFlags = kAllStages});
			}

			const std::vector<vk::DescriptorBindingFlags> flags(bindings.size(), vk::DescriptorBindingFlags{});
			const std::string name = std::format("{}:PrivateSet[{}]", debugName, set);

			vk::DescriptorSetLayout layout = CreateSetLayout(bindings, flags, false, name.c_str());
			if (!layout)
				return nullptr;

			s_PrivateSetLayouts.push_back(layout);
			layouts.push_back(layout);
		}

		const vk::PushConstantRange pushRange{.stageFlags = kAllStages, .offset = 0, .size = kPushConstantBytes};

		auto layout = VulkanContext::Get().GetDevice().createPipelineLayout(
			{.setLayoutCount = (uint32_t)layouts.size(), .pSetLayouts = layouts.data(), .pushConstantRangeCount = 1, .pPushConstantRanges = &pushRange});
		if (!layout)
		{
			TF_CORE_ERROR("createPipelineLayout failed for '{0}': {1}", debugName, vk::to_string(layout.error()));
			return nullptr;
		}

		VulkanContext::Get().SetObjectName(*layout, std::format("{}:Layout", debugName));
		s_LayoutCache.emplace(key, *layout);

		TF_CORE_INFO("Pipeline layout cache: new entry for '{0}' with {1} private set(s), {2} entries total", debugName,
			highestSet + 1 - SetCount, s_LayoutCache.size());

		return *layout;
	}

	void VulkanBindings::BindGlobalSets(vk::CommandBuffer cmd, vk::PipelineLayout layout, uint32_t passSlice)
	{
		if (!s_Ready || !layout)
			return;

		const uint32_t dynamicOffsets[]{(uint32_t)(s_Slot * s_FrameUniformStride),
			(uint32_t)((s_Slot * RHI::PassUniformSlices + std::min(passSlice, RHI::PassUniformSlices - 1)) * s_PassUniformStride)};

		cmd.bindDescriptorSets2({.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
			.layout = layout,
			.firstSet = 0,
			.descriptorSetCount = SetCount,
			.pDescriptorSets = s_Sets.data(),
			.dynamicOffsetCount = (uint32_t)std::size(dynamicOffsets),
			.pDynamicOffsets = dynamicOffsets});
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

	uint32_t RHI::Bindings::WritePassUniforms(const void* data, uint64_t size)
	{
		if (!s_Ready || !data)
			return UINT32_MAX;

		if (size > RHI::PassUniformSlotBytes)
		{
			TF_CORE_ERROR("WritePassUniforms got {0} bytes for a {1} byte slot", size, RHI::PassUniformSlotBytes);
			return UINT32_MAX;
		}

		if (s_PassSlice >= RHI::PassUniformSlices)
		{
			if (!s_PassSliceExhausted)
			{
				TF_CORE_ERROR("Pass uniform slices exhausted ({0} per frame)", RHI::PassUniformSlices);
				s_PassSliceExhausted = true;
			}

			return UINT32_MAX;
		}

		const uint32_t slice = s_PassSlice++;
		const uint64_t offset = (s_Slot * RHI::PassUniformSlices + slice) * s_PassUniformStride;
		return s_PassUniforms->Write(data, size, offset) ? slice : UINT32_MAX;
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
