#pragma once

// Shared pimpl definitions for the Vulkan RHI backend. RHI public headers (Timefall/RHI/*.h)
// forward-declare each ::Impl to stay API-agnostic; backend .cpp files that need to touch more
// than one RHI object's internals include this header instead of redefining structs locally.

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/GpuBuffer.h"

namespace Timefall::RHI
{
	struct CommandList::Impl
	{
		vk::CommandBuffer Cmd;
		vk::Image TargetImage;
		vk::ImageView TargetView;
		vk::Extent2D TargetExtent;
		vk::ImageLayout* TargetLayout = nullptr; // points at the owner's layout slot, updated in place
		bool InPass = false;

		vk::PipelineLayout BoundLayout;
		vk::ShaderStageFlags BoundPushStages;
		uint32_t BoundPushSize = 0;
	};

	struct GraphicsPipeline::Impl
	{
		GraphicsPipelineDesc Desc;
		vk::Pipeline Pipeline;
		vk::PipelineLayout Layout;
		std::vector<vk::DescriptorSetLayout> SetLayouts;
		vk::ShaderStageFlags PushStages;
		uint32_t PushSize = 0;
		uint32_t BuiltRevision = 0;
	};

	struct GpuBuffer::Impl
	{
		vk::Buffer Buffer;
		VmaAllocation Allocation = nullptr;
		void* Mapped = nullptr;
		uint64_t Size = 0;
		uint32_t TrackerId = 0;
	};
}
