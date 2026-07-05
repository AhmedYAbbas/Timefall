#pragma once

#include "Timefall/Core/Core.h"

#include <cstdint>

namespace Timefall
{
	// RGBA16F cubemap with an optional mip chain. The IBL bake passes attach individual
	// (face, mip) images to an FBO as render targets; sampling uses cube filtering.
	class TF_API TextureCube
	{
	public:
		virtual ~TextureCube() = default;

		virtual void BindForSampling(uint32_t slot) const = 0;

		virtual uint32_t GetRendererID() const = 0;
		virtual uint32_t GetSize() const = 0;
		virtual uint32_t GetMipLevels() const = 0;

		static Ref<TextureCube> Create(uint32_t size, uint32_t mipLevels = 1);
	};
}
