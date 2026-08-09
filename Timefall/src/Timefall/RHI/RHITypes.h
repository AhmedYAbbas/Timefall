#pragma once

#include <cstdint>

namespace Timefall::RHI
{
	struct TF_API Limits
	{
		uint32_t MaxBindlessTextures = 0;
		uint32_t MaxPushConstantSize = 0;
		uint64_t MinUniformBufferOffsetAlignment = 0;
		uint64_t MinStorageBufferOffsetAlignment = 0;
		float MaxSamplerAnisotropy = 0.0f;
		bool SupportsRobustness2 = false;
	};

	inline constexpr uint32_t TF_BINDLESS_TEXTURE_BUDGET = 16384;
	inline constexpr uint32_t FRAMES_IN_FLIGHT = 2;

	enum class Format { Undefined = 0, RGBA8Unorm,BGRA8Unorm, RGBA16F, R32I, D32F };
	enum class LoadOp { Load = 0, Clear, DontClear };
	enum class StoreOp { Store = 0, DontCare };

	struct ColorTarget
	{
		void* Image = nullptr; // nullptr == current swap chain image
		LoadOp Load = LoadOp::Clear;
		StoreOp Store = StoreOp::Store;
		float ClearValue[4]{0.0f, 0.0f, 0.0f, 1.0f};
	};

	struct PassDesc
	{
		const char* DebugName = nullptr;
		ColorTarget Color[8]{};
		uint32_t ColorCount = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};
}
