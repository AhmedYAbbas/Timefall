#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

namespace Timefall::RHI
{
	class GraphicsPipeline;

	class TF_API CommandList
	{
	public:
		void BeginPass(const PassDesc& desc);
		void EndPass();

		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

		void BindPipeline(const GraphicsPipeline& pipeline);

		void PushConstants(const void* data, uint32_t size, uint32_t offset = 0);

		void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0);

		void* GetNativeHandle();

	private:
		friend class RenderDevice;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
