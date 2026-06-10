#pragma once

#include "Timefall/Core/Core.h"

namespace Timefall
{
	class TF_API UniformBuffer
	{
	public:
		virtual ~UniformBuffer() = default;

		// offset/size in bytes; uploads into the GPU buffer at the given byte offset.
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;

		// binding = std140 binding point referenced by `layout(std140, binding = N)` in shaders.
		static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
	};
}
