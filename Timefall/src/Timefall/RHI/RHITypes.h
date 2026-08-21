#pragma once

#include <cstdint>

namespace Timefall::RHI
{
	struct Limits
	{
		uint32_t MaxBindlessTextures = 0;
		uint32_t MaxPushConstantSize = 0;
		uint64_t MinUniformBufferOffsetAlignment = 0;
		uint64_t MinStorageBufferOffsetAlignment = 0;
		float MaxSamplerAnisotropy = 0.0f;
		bool SupportsRobustness2 = false;
		bool SupportsWideLines = false;
		bool SupportsSmoothLines = false;
	};

	inline constexpr uint32_t TF_BINDLESS_TEXTURE_BUDGET = 16384;
	inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;

	enum class Format { Undefined = 0, R8Unorm, RGBA8Unorm, RGBA8Srgb, BGRA8Unorm, RGBA16F, RGBA32F, R32I, D32F };
	enum class LoadOp { Load = 0, Clear, DontCare };
	enum class StoreOp { Store = 0, DontCare };
	enum class IndexType : uint8_t { U16 = 0, U32 };
	enum class TextureUsage : uint32_t { None = 0, Sampled = BIT(0), ColorAttachment = BIT(1), DepthAttachment = BIT(2), TransferSrc = BIT(3), TransferDst = BIT(4) };

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
		ColorTarget Color[8]{};
		DepthTarget Depth{};
	};
}
