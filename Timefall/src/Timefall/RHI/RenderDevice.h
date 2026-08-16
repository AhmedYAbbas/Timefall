#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

#include <functional>

namespace Timefall::RHI
{
	class CommandList;

	class TF_API RenderDevice
	{
	public:
		static RenderDevice& Get();

		void Init(void* nativeWindow);
		void Shutdown();

		const Limits& GetLimits() const;

		// Returns nullptr when the swapchain is being rebuilt - skip the frame entirely.
		CommandList* BeginFrame();
		void EndFrame();

		// Valid only between BeginFrame and EndFrame; nullptr otherwise.
		CommandList* GetCurrentCommandList();

		void OnResize(uint32_t width, uint32_t height);
		void SetVSync(bool enabled);
		void WaitIdle();

		void DeferDestroy(std::function<void()>&& fn);

		Format GetSwapchainColorFormat() const;

		uint32_t GetSwapchainFormat() const;
		uint32_t GetSwapchainImageCount() const;
		uint32_t GetSwapchainMinImageCount() const;

	private:
		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
