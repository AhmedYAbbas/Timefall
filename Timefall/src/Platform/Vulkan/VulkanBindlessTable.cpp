#include "tfpch.h"
#include "VulkanBindlessTable.h"

#include "Platform/Vulkan/VulkanBindings.h"
#include "Platform/Vulkan/VulkanContext.h"

namespace Timefall
{
	namespace
	{
		struct BindlessTableData
		{
			vk::DescriptorSet Set;
			vk::ImageView WhiteView;
			uint32_t Capacity = 0;
			uint32_t Next = 1;
			std::vector<uint32_t> Free;
			uint32_t Used = 0;
			bool Ready = false;
			bool ExhaustionLogged = false;
		};

		BindlessTableData s_TableData{};

		void WriteSlot(uint32_t index, vk::ImageView view)
		{
			const vk::DescriptorImageInfo info{.imageView = view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

			const vk::WriteDescriptorSet write{.dstSet = s_TableData.Set,
				.dstBinding = VulkanBindings::BindingTextures,
				.dstArrayElement = index,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &info};

			VulkanContext::Get().GetDevice().updateDescriptorSets(write, {});
		}
	}

	void VulkanBindlessTable::Init(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteView)
	{
		TF_PROFILE_FUNCTION();

		if (!set || !whiteView || capacity == 0)
		{
			TF_CORE_ERROR("VulkanBindlessTable::Init got an incomplete set, view or capacity");
			return;
		}

		s_TableData.Set = set;
		s_TableData.WhiteView = whiteView;
		s_TableData.Capacity = capacity;
		s_TableData.Next = 1;
		s_TableData.Free.clear();
		s_TableData.Used = 1;
		s_TableData.ExhaustionLogged = false;

		const std::vector<vk::DescriptorImageInfo> infos(
			capacity, vk::DescriptorImageInfo{.imageView = whiteView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal});

		const vk::WriteDescriptorSet fill{.dstSet = s_TableData.Set,
			.dstBinding = VulkanBindings::BindingTextures,
			.dstArrayElement = 0,
			.descriptorCount = capacity,
			.descriptorType = vk::DescriptorType::eSampledImage,
			.pImageInfo = infos.data()};

		VulkanContext::Get().GetDevice().updateDescriptorSets(fill, {});

		s_TableData.Ready = true;
		TF_CORE_INFO("Bindless table: {0} slots pre-filled with the white texture", capacity);
	}

	void VulkanBindlessTable::Shutdown()
	{
		s_TableData.Ready = false;
		s_TableData.Set = nullptr;
		s_TableData.WhiteView = nullptr;
		s_TableData.Capacity = 0;
		s_TableData.Next = 1;
		s_TableData.Free.clear();
		s_TableData.Used = 0;
	}

	uint32_t VulkanBindlessTable::Acquire(vk::ImageView view)
	{
		if (!s_TableData.Ready || !view)
			return InvalidIndex;

		uint32_t index = InvalidIndex;
		if (!s_TableData.Free.empty())
		{
			index = s_TableData.Free.back();
			s_TableData.Free.pop_back();
		}
		else if (s_TableData.Next < s_TableData.Capacity)
		{
			index = s_TableData.Next++;
		}
		else
		{
			if (!s_TableData.ExhaustionLogged)
			{
				TF_CORE_ERROR("Bindless table exhausted at {0} slots; further textures sample white", s_TableData.Capacity);
				s_TableData.ExhaustionLogged = true;
			}

			return InvalidIndex;
		}

		WriteSlot(index, view);
		s_TableData.Used++;
		return index;
	}

	void VulkanBindlessTable::Release(uint32_t index)
	{
		if (!s_TableData.Ready || index == InvalidIndex || index == WhiteIndex || index >= s_TableData.Capacity)
			return;

		WriteSlot(index, s_TableData.WhiteView);
		s_TableData.Free.push_back(index);
		s_TableData.Used--;
	}

	uint32_t VulkanBindlessTable::GetCapacity()
	{
		return s_TableData.Capacity;
	}

	uint32_t VulkanBindlessTable::GetUsed()
	{
		return s_TableData.Used;
	}
}
