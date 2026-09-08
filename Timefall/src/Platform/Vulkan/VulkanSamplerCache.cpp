#include "tfpch.h"
#include "VulkanSamplerCache.h"

#include "Platform/Vulkan/VulkanContext.h"

namespace Timefall
{
	namespace
	{
		std::vector<std::pair<VulkanSamplerCache::SamplerDesc, vk::Sampler>> s_Cache;
		std::array<vk::Sampler, (size_t)RHI::SamplerSlot::Count> s_Table{};
		std::array < vk::Sampler, (size_t)RHI::ComparisonSamplerSlot::Count> s_CmpTable{};

		const char* SlotName(RHI::SamplerSlot slot)
		{
			switch (slot)
			{
				case RHI::SamplerSlot::LinearRepeat: return "LinearRepeat";
				case RHI::SamplerSlot::LinearClampEdge: return "LinearClampEdge";
				case RHI::SamplerSlot::NearestClampEdge: return "NearestClampEdge";
				default: return "<unknown>";
			}
		}

		const char* CmpSlotName(RHI::ComparisonSamplerSlot slot)
		{
			switch (slot)
			{
				case RHI::ComparisonSamplerSlot::ShadowLinearClamp: return "ShadowLinearClamp";
				default: return "<unknown>";
			}
		}
	}

	vk::Sampler VulkanSamplerCache::Get(const SamplerDesc& desc)
	{
		if (auto it = std::ranges::find_if(s_Cache, [&desc](const auto& entry) { return entry.first == desc; }); it != s_Cache.end())
			return it->second;

		auto& ctx = VulkanContext::Get();

		const vk::SamplerCreateInfo info{.magFilter = desc.MinMagFilter,
			.minFilter = desc.MinMagFilter,
			.mipmapMode = desc.MipMode,
			.addressModeU = desc.AddressMode,
			.addressModeV = desc.AddressMode,
			.addressModeW = desc.AddressMode,
			.mipLodBias = 0.0f,
			.anisotropyEnable = desc.Anisotropic ? vk::True : vk::False,
			.maxAnisotropy = desc.Anisotropic ? ctx.GetLimits().MaxSamplerAnisotropy : 1.0f,
			.compareEnable = desc.CompareDepth ? vk::True : vk::False,
			.compareOp = desc.CompareDepth ? vk::CompareOp::eLessOrEqual : vk::CompareOp::eNever,
			.minLod = 0.0f,
			.maxLod = vk::LodClampNone,
			.borderColor = vk::BorderColor::eFloatOpaqueWhite,
			.unnormalizedCoordinates = vk::False};

		auto sampler = ctx.GetDevice().createSampler(info);
		if (!sampler)
		{
			TF_CORE_ERROR("createSampler failed: {0}", vk::to_string(sampler.error()));
			return nullptr;
		}

		s_Cache.emplace_back(desc, *sampler);
		return *sampler;
	}

	vk::Sampler VulkanSamplerCache::Get(RHI::SamplerSlot slot)
	{
		const size_t index = (size_t)slot;
		return index < s_Table.size() ? s_Table[index] : nullptr;
	}

	vk::Sampler VulkanSamplerCache::Get(RHI::ComparisonSamplerSlot slot)
	{
		const size_t index = (size_t)slot;
		return index < s_CmpTable.size() ? s_CmpTable[index] : nullptr;
	}

	void VulkanSamplerCache::Init()
	{
		s_Table[(size_t)RHI::SamplerSlot::LinearRepeat] = Get({.MinMagFilter = vk::Filter::eLinear,
			.MipMode = vk::SamplerMipmapMode::eLinear,
			.AddressMode = vk::SamplerAddressMode::eRepeat,
			.Anisotropic = true});

		s_Table[(size_t)RHI::SamplerSlot::LinearClampEdge] = Get({.MinMagFilter = vk::Filter::eLinear,
			.MipMode = vk::SamplerMipmapMode::eLinear,
			.AddressMode = vk::SamplerAddressMode::eClampToEdge});

		s_Table[(size_t)RHI::SamplerSlot::NearestClampEdge] = Get({.MinMagFilter = vk::Filter::eNearest,
			.MipMode = vk::SamplerMipmapMode::eNearest,
			.AddressMode = vk::SamplerAddressMode::eClampToEdge});

		s_CmpTable[(size_t)RHI::ComparisonSamplerSlot::ShadowLinearClamp] = Get({.MinMagFilter = vk::Filter::eLinear,
			.MipMode = vk::SamplerMipmapMode::eNearest,
			.AddressMode = vk::SamplerAddressMode::eClampToEdge,
			.Anisotropic = false,
			.CompareDepth = true});

		auto& ctx = VulkanContext::Get();
		for (uint32_t i = 0; i < (uint32_t)RHI::SamplerSlot::Count; i++)
		{
			if (!s_Table[i])
			{
				TF_CORE_ERROR("Sampler slot {0} failed to build", SlotName((RHI::SamplerSlot)i));
				continue;
			}

			ctx.SetObjectName(s_Table[i], std::format("Sampler:{}", SlotName((RHI::SamplerSlot)i)));
		}

		for (uint32_t i = 0; i < (uint32_t)RHI::ComparisonSamplerSlot::Count; i++)
		{
			if (!s_CmpTable[i])
			{
				TF_CORE_ERROR("Comparison sampler slot {0} failed to build", CmpSlotName((RHI::ComparisonSamplerSlot)i));
				continue;
			}

			ctx.SetObjectName(s_CmpTable[i], std::format("ComparisonSampler:{}", CmpSlotName((RHI::ComparisonSamplerSlot)i)));
		}

		TF_CORE_INFO("Sampler cache: {0} canonical slots, {1} distinct samplers", (uint32_t)RHI::SamplerSlot::Count, s_Cache.size());
	}

	void VulkanSamplerCache::Shutdown()
	{
		auto device = VulkanContext::Get().GetDevice();
		for (auto& [desc, sampler] : s_Cache)
			device.destroySampler(sampler);

		s_Cache.clear();
		s_Table.fill(nullptr);
	}
}
