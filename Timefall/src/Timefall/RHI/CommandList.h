#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

namespace Timefall::RHI
{
	class TF_API CommandList
	{
	public:
		void BeginPass(const PassDesc& desc);
		void EndPass();

		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		void SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

		void* GetNativeHandle();

	private:
		friend class RenderDevice;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
