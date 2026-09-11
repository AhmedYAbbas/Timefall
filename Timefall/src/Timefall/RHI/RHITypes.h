#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace Timefall::RHI
{
	struct Limits
	{
		uint32_t MaxBindlessTextures = 0;
		uint32_t MaxPushConstantSize = 0;
		uint64_t MinUniformBufferOffsetAlignment = 0;
		uint64_t MinStorageBufferOffsetAlignment = 0;
		uint32_t MaxMultiviewViews = 0;
		float MaxSamplerAnisotropy = 0.0f;
		bool SupportsRobustness2 = false;
		bool SupportsWideLines = false;
		bool SupportsSmoothLines = false;
		std::string DeviceName;
		std::string DriverInfo;
	};

	inline constexpr uint32_t TF_BINDLESS_TEXTURE_BUDGET = 16384;
	inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;

	enum class Format { Undefined = 0, R8Unorm, RGBA8Unorm, RGBA8Srgb, BGRA8Unorm, RGBA16F, RGBA32F, R32I, D32F };
	enum class LoadOp { Load = 0, Clear, DontCare };
	enum class StoreOp { Store = 0, DontCare };
	enum class IndexType : uint8_t { U16 = 0, U32 };
	enum class TextureUsage : uint32_t { None = 0, Sampled = BIT(0), ColorAttachment = BIT(1), DepthAttachment = BIT(2), TransferSrc = BIT(3), TransferDst = BIT(4) };
	enum class Dimension : uint8_t { Tex2D = 0, Tex2DArray, Cube, CubeArray };

	constexpr TextureUsage operator|(TextureUsage a, TextureUsage b)
	{
		return (TextureUsage)((uint32_t)a | (uint32_t)b);
	}

	constexpr bool HasFlag(TextureUsage value, TextureUsage flag)
	{
		return ((uint32_t)value & (uint32_t)flag) != 0;
	}

	class RenderTarget;

	struct ColorTarget
	{
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		float ClearValue[4]{0.0f, 0.0f, 0.0f, 1.0f};
		int32_t ClearInt[4]{};
	};

	struct DepthTarget
	{
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		float ClearDepth = 1.0f;
	};

	struct PassDesc
	{
		const char* DebugName = nullptr;
		RenderTarget* Target = nullptr;
		uint32_t Layer = 0; // array layer / cube face
		uint32_t Mip = 0;
		uint32_t ViewCount = 1; // > 1 renders Layer..Layer+ViewCount-1 at once, SV_ViewID selects
		ColorTarget Color[8]{};
		DepthTarget Depth{};
	};

	// One (layer, mip) slice of an image, optionally narrowed to a pixel rectangle (CopyTextureToBuffer only)
	struct TextureRegion
	{
		uint32_t Layer = 0; // array layer / cube face
		uint32_t Mip = 0;
		uint32_t X = 0;
		uint32_t Y = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	struct TextureRect
	{
		uint32_t X = 0;
		uint32_t Y = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	constexpr std::optional<TextureRect> ResolveRegion(const TextureRegion& region, uint32_t width, uint32_t height)
	{
		const uint32_t mipWidth = (width >> region.Mip) ? (width >> region.Mip) : 1u;
		const uint32_t mipHeight = (height >> region.Mip) ? (height >> region.Mip) : 1u;
		if (region.X >= mipWidth || region.Y >= mipHeight)
			return std::nullopt;

		const uint32_t w = region.Width ? region.Width : mipWidth - region.X;
		const uint32_t h = region.Height ? region.Height : mipHeight - region.Y;
		if (w > mipWidth - region.X || h > mipHeight - region.Y)
			return std::nullopt;

		return TextureRect{.X = region.X, .Y = region.Y, .Width = w, .Height = h};
	}

	constexpr uint32_t BytesPerPixel(Format format)
	{
		switch (format)
		{
			case Format::R8Unorm:	return 1;
			case Format::RGBA8Unorm:
			case Format::RGBA8Srgb:
			case Format::BGRA8Unorm:
			case Format::R32I:
			case Format::D32F:		return 4;
			case Format::RGBA16F:	return 8;
			case Format::RGBA32F:	return 16;
			default:				return 0;
		}
	}
}
