#pragma once

// Shared pimpl definitions for the Vulkan RHI backend. RHI public headers (Timefall/RHI/*.h)
// forward-declare each ::Impl to stay API-agnostic; backend .cpp files that need to touch more
// than one RHI object's internals include this header instead of redefining structs locally.

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/Pipeline.h"
#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/Texture.h"
#include "Timefall/RHI/RenderTarget.h"

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

	inline bool IsCubeDimension(Dimension dim)
	{
		return dim == Dimension::Cube || dim == Dimension::CubeArray;
	}

	inline vk::ImageViewType ToVkImageViewType(Dimension dim)
	{
		switch (dim)
		{
			case Dimension::Tex2DArray: return vk::ImageViewType::e2DArray;
			case Dimension::Cube:		return vk::ImageViewType::eCube;
			case Dimension::CubeArray:	return vk::ImageViewType::eCubeArray;
			default:					return vk::ImageViewType::e2D;
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

	inline bool IsDepthFormat(Format format)
	{
		return format == Format::D32F;
	}

	inline bool IsIntegerFormat(Format format)
	{
		return format == Format::R32I;
	}

	struct CommandList::Impl
	{
		vk::CommandBuffer Cmd;

		vk::Image SwapImage;
		vk::ImageView SwapView;
		vk::Extent2D SwapExtent;
		vk::ImageLayout* SwapLayout = nullptr;

		bool InPass = false;
		bool PassLabelPushed = false;
		RenderTarget* PassTarget = nullptr;
		uint32_t PassColorCount = 0;
		uint32_t PassLayer = 0;
		uint32_t PassMip = 0;
		uint32_t PassViewCount = 1;
		uint32_t PassSlice = 0;

		vk::PipelineLayout BoundLayout;
		vk::ShaderStageFlags BoundPushStages;
		uint32_t BoundPushSize = 0;
	};

	struct GraphicsPipeline::Impl
	{
		GraphicsPipelineDesc Desc;
		vk::Pipeline Pipeline;
		vk::PipelineLayout Layout;
		vk::ShaderStageFlags PushStages;
		uint32_t PushSize = 0;
		uint32_t BuiltRevision = 0;
		bool HasGlobalPrefix = false;
	};

	struct GpuBuffer::Impl
	{
		vk::Buffer Buffer;
		VmaAllocation Allocation = nullptr;
		void* Mapped = nullptr;
		uint64_t Size = 0;
		uint32_t TrackerId = 0;
		bool DeviceAddressable = false;
	};

	struct Texture::Impl
	{
		vk::Image Image;
		vk::ImageView View;
		vk::ImageView SRGBView;
		std::vector<vk::ImageView> SubresourceViews; // lazily build, mip * ArrayLayers + layer

		struct ArrayViewEntry
		{
			uint32_t Mip;
			uint32_t BaseLayer;
			uint32_t LayerCount;
			vk::ImageView View;
		};
		std::vector<ArrayViewEntry> ArrayViews; // lazily build, mip * ArrayLayers + layer (sparse)

		VmaAllocation Allocation = nullptr;
		vk::Format Format = vk::Format::eUndefined;
		RHI::Format PixelFormat = RHI::Format::Undefined;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t MipLevels = 1;
		uint32_t TrackerId = 0;
		uint32_t BindlessIndex = UINT32_MAX;
		uint32_t BindlessSRGBIndex = UINT32_MAX;
		bool HostCopyable = false;
		vk::ImageAspectFlagBits Aspect = vk::ImageAspectFlagBits::eColor;

		uint32_t ArrayLayers = 1;
		std::vector<vk::ImageLayout> Layouts; // mip * ArrayLayers + Layer
		Dimension Dim = Dimension::Tex2D;
		bool LinearBlit = false;

		bool IsAttachment = false;
		vk::ImageUsageFlags UsageFlags{};
		void* UIHandle = nullptr;
		void (*UIHandleDestroy)(void*) = nullptr;

		//Single-(mip, layer) 2D view for use as a render-pass attachment. Built on first use
		vk::ImageView SubresourceView(uint32_t mip, uint32_t layer);
		vk::ImageView SubresourceArrayView(uint32_t mip, uint32_t baseLayer, uint32_t layerCount);

		vk::ImageLayout& LayoutAt(uint32_t mip, uint32_t layer)
		{
			return Layouts[(size_t)mip * ArrayLayers + layer];
		}

		void SetAllLayouts(vk::ImageLayout layout)
		{
			std::ranges::fill(Layouts, layout);
		}

		vk::ImageSubresourceRange FullRange() const
		{
			return {Aspect, 0, MipLevels, 0, ArrayLayers};
		}
	};
}
