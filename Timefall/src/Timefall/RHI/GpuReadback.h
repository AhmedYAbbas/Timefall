#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/GpuBuffer.h"
#include "Timefall/RHI/RHITypes.h"

#include <span>

namespace Timefall::RHI
{
	class CommandList;
	class Texture;

	struct ReadbackResult
	{
		std::span<const std::byte> Bytes; // tightly packed rows; valid until the next Request
		uint32_t Width = 0;
		uint32_t Height = 0;
		Format PixelFormat = Format::Undefined;
	};

	// Texture-to-CPU copies on the frame clock: a copy recorded in frame F is readable once F retires. No fences
	class TF_API GpuReadback
	{
	public:
		explicit GpuReadback(const char* debugName = "GpuReadback");

		// Outside a pass only. False when this frame already has a request or nothing could be recorded
		bool Request(CommandList& cmd, Texture& src, const TextureRegion& region);

		// The newest request whose frame has retired, returned once; older retired requests are dropped
		std::optional<ReadbackResult> Poll();

		bool IsPending() const;

	private:
		struct Slot
		{
			Ref<GpuBuffer> Buffer;
			uint64_t FrameValue = 0; // 0 = empty
			uint32_t Width = 0;
			uint32_t Height = 0;
			Format PixelFormat = Format::Undefined;
		};

		std::array<Slot, FRAMES_IN_FLIGHT> m_Slots{};
		const char* m_DebugName;
	};
}
