#pragma once

// Shared pimpl definitions for the Vulkan RHI backend. RHI public headers (Timefall/RHI/*.h)
// forward-declare each ::Impl to stay API-agnostic; backend .cpp files that need to touch more
// than one RHI object's internals include this header instead of redefining structs locally.

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Texture.h"

namespace Timefall::RHI
{
	inline vk::Format ToVkFormat(Format format)
	{
		switch (format)
		{
			case Format::R8Unorm:	 return vk::Format::eR8Unorm;
			case Format::RGBA8Unorm: return vk::Format::eR8G8B8A8Unorm;
			case Format::RGBA8Srgb:  return vk::Format::eR8G8B8A8Srgb;
			case Format::BGRA8Unorm: return vk::Format::eB8G8R8A8Unorm;
			case Format::RGBA16F:	 return vk::Format::eR16G16B16A16Sfloat;
			case Format::RGBA32F:	 return vk::Format::eR32G32B32A32Sfloat;
			case Format::R32I:		 return vk::Format::eR32Sint;
			case Format::D32F:		 return vk::Format::eD32Sfloat;
			default:				 return vk::Format::eUndefined;
		}
	}

	inline uint32_t BytesPerPixel(Format format)
	{
		switch (format)
		{
			case Format::R8Unorm:	 return 1;
			case Format::RGBA8Unorm:
			case Format::RGBA8Srgb:
			case Format::BGRA8Unorm:
			case Format::R32I:
			case Format::D32F:		 return 4;
			case Format::RGBA16F:	 return 8;
			case Format::RGBA32F:	 return 16;
			default:				 return 0;
		}
	}

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

	struct Texture::Impl
	{
		vk::Image Image;
		vk::ImageView View;
		vk::ImageView SRGBView;

		VmaAllocation Allocation = nullptr;
		vk::Format Format = vk::Format::eUndefined;
		RHI::Format PixelFormat = RHI::Format::Undefined;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipLevels = 1;
		uint32_t TrackerId = 0;
		bool HostCopyable = false;
		void* UIHandle = nullptr;
		void (*UIHandleDestroy)(void*) = nullptr;
	};
}
