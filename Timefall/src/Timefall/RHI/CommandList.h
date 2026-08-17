#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

namespace Timefall::RHI
{
	class GraphicsPipeline;
	class GpuBuffer;

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

		void BindVertexBuffer(const GpuBuffer& buffer, uint64_t offset = 0);
		void BindIndexBuffer(const GpuBuffer& buffer, IndexType type, uint64_t offset = 0);

		void DrawIndexed(
			uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0);

		void* GetNativeHandle();

	private:
		friend class RenderDevice;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
