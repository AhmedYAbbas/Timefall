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
			uint32_t Binding = 0;
			const char* Label = "Bindless table";
			bool Ready = false;
			bool ExhaustionLogged = false;
		};

		BindlessTableData s_TableData{};
		BindlessTableData s_CubeData{};
		BindlessTableData s_ArrayData{};
		BindlessTableData s_CubeArrayData{};

		void WriteSlot(BindlessTableData& table, uint32_t index, vk::ImageView view)
		{
			const vk::DescriptorImageInfo info{.imageView = view, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

			const vk::WriteDescriptorSet write{.dstSet = table.Set,
				.dstBinding = table.Binding,
				.dstArrayElement = index,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = &info};

			VulkanContext::Get().GetDevice().updateDescriptorSets(write, {});
		}

		void InitTable(BindlessTableData& table, vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteView, const char* label)
		{
			TF_PROFILE_FUNCTION();

			table.Label = label;

			if (!set || !whiteView || capacity == 0)
			{
				TF_CORE_ERROR("{0}: Init got an incomplete set, view or capacity", label);
				return;
			}

			table.Set = set;
			table.WhiteView = whiteView;
			table.Capacity = capacity;
			table.Next = 1;
			table.Free.clear();
			table.Used = 1;
			table.ExhaustionLogged = false;

			const std::vector<vk::DescriptorImageInfo> infos(
				capacity, vk::DescriptorImageInfo{.imageView = whiteView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal});

			const vk::WriteDescriptorSet fill{.dstSet = table.Set,
				.dstBinding = table.Binding,
				.dstArrayElement = 0,
				.descriptorCount = capacity,
				.descriptorType = vk::DescriptorType::eSampledImage,
				.pImageInfo = infos.data()};

			VulkanContext::Get().GetDevice().updateDescriptorSets(fill, {});

			table.Ready = true;
			TF_CORE_INFO("{0}: {1} slots pre-filled with the white texture", label, capacity);
		}

		void ResetTable(BindlessTableData& table)
		{
			table.Ready = false;
			table.Set = nullptr;
			table.WhiteView = nullptr;
			table.Capacity = 0;
			table.Next = 1;
			table.Free.clear();
			table.Used = 0;
		}

		uint32_t AcquireIn(BindlessTableData& table, vk::ImageView view)
		{
			if (!table.Ready || !view)
				return VulkanBindlessTable::InvalidIndex;

			uint32_t index = VulkanBindlessTable::InvalidIndex;
			if (!table.Free.empty())
			{
				index = table.Free.back();
				table.Free.pop_back();
			}
			else if (table.Next < table.Capacity)
			{
				index = table.Next++;
			}
			else
			{
				if (!table.ExhaustionLogged)
				{
					TF_CORE_ERROR("{0} exhausted at {1} slots; further textures sample white", table.Label, table.Capacity);
					table.ExhaustionLogged = true;
				}

				return VulkanBindlessTable::InvalidIndex;
			}

			WriteSlot(table, index, view);
			table.Used++;
			return index;
		}

		void ReleaseIn(BindlessTableData& table, uint32_t index)
		{
			if (!table.Ready || index == VulkanBindlessTable::InvalidIndex || index == VulkanBindlessTable::WhiteIndex
				|| index >= table.Capacity)
				return;

			WriteSlot(table, index, table.WhiteView);
			table.Free.push_back(index);
			table.Used--;
		}
	}

	void VulkanBindlessTable::Init(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteView)
	{
		s_TableData.Binding = VulkanBindings::BindingTextures;
		InitTable(s_TableData, set, capacity, whiteView, "Bindless table");
	}

	void VulkanBindlessTable::InitCubes(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteCubeView)
	{
		s_CubeData.Binding = VulkanBindings::BindingTextureCubes;
		InitTable(s_CubeData, set, capacity, whiteCubeView, "Bindless cube table");
	}

	void VulkanBindlessTable::InitArrays(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteArrayView)
	{
		s_ArrayData.Binding = VulkanBindings::BindingTextureArrays;
		InitTable(s_ArrayData, set, capacity, whiteArrayView, "Bindless array table");
	}

	void VulkanBindlessTable::InitCubeArrays(vk::DescriptorSet set, uint32_t capacity, vk::ImageView whiteCubeArrayView)
	{
		s_CubeArrayData.Binding = VulkanBindings::BindingTextureCubeArrays;
		InitTable(s_CubeArrayData, set, capacity, whiteCubeArrayView, "Bindless cube-array table");
	}

	void VulkanBindlessTable::Shutdown()
	{
		ResetTable(s_TableData);
		ResetTable(s_CubeData);
		ResetTable(s_ArrayData);
		ResetTable(s_CubeArrayData);
	}

	uint32_t VulkanBindlessTable::Acquire(vk::ImageView view)
	{
		return AcquireIn(s_TableData, view);
	}

	void VulkanBindlessTable::Release(uint32_t index)
	{
		ReleaseIn(s_TableData, index);
	}

	uint32_t VulkanBindlessTable::AcquireCube(vk::ImageView view)
	{
		return AcquireIn(s_CubeData, view);
	}

	void VulkanBindlessTable::ReleaseCube(uint32_t index)
	{
		ReleaseIn(s_CubeData, index);
	}

	uint32_t VulkanBindlessTable::AcquireArray(vk::ImageView view)
	{
		return AcquireIn(s_ArrayData, view);
	}

	void VulkanBindlessTable::ReleaseArray(uint32_t index)
	{
		ReleaseIn(s_ArrayData, index);
	}

	uint32_t VulkanBindlessTable::AcquireCubeArray(vk::ImageView view)
	{
		return AcquireIn(s_CubeArrayData, view);
	}

	void VulkanBindlessTable::ReleaseCubeArray(uint32_t index)
	{
		ReleaseIn(s_CubeArrayData, index);
	}

	uint32_t VulkanBindlessTable::GetCapacity()
	{
		return s_TableData.Capacity;
	}

	uint32_t VulkanBindlessTable::GetUsed()
	{
		return s_TableData.Used;
	}

	uint32_t VulkanBindlessTable::GetCubeUsed()
	{
		return s_CubeData.Used;
	}

	uint32_t VulkanBindlessTable::GetArrayUsed()
	{
		return s_ArrayData.Used;
	}

	uint32_t VulkanBindlessTable::GetCubeArrayUsed()
	{
		return s_CubeArrayData.Used;
	}
}
