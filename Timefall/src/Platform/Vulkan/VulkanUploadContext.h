#pragma once

#include <vulkan/vulkan.hpp>

#include <functional>

namespace Timefall
{
	// Transfer submits for resource initialisation, over one persistent staging ring. Blocking by
	// design: it waits on its own timeline semaphore, never the frame ring's, so an upload cannot be
	// mistaken for a retired frame. Init/load-time only - the stall makes it unusable per-frame.
	//
	// Not thread-safe: all state is file-scope and unguarded. Correct while the engine is
	// single-threaded; an asset-loading thread must not reach this before it grows a lock.
	class VulkanUploadContext
	{
	public:
		static void Init(uint64_t stagingBytes = 32 * 1024 * 1024);
		static void Shutdown();

		// Batch bracket, refcounted - only the outermost End flushes. Reach these through
		// RHI::UploadScope rather than calling them directly.
		static void Begin();
		static void End();

		// Stages `size` bytes and records the copy into `dst`. Chunked when it exceeds the ring.
		static bool UploadBuffer(vk::Buffer dst, uint64_t dstOffset, const void* data, uint64_t size);

		// Escape hatch for transfers this API does not model - image copies, blits, layout
		// transitions. Runs against the batch's command buffer, and does NOT chunk: a payload
		// larger than the ring must be split by the caller.
		static void Record(const std::function<void(vk::CommandBuffer)>& fn);

		// Submits whatever is recorded and waits for it. A no-op when nothing is recorded.
		static void Flush();
	};
}
