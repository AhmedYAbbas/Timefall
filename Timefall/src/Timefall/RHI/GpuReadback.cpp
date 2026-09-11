#include "tfpch.h"
#include "GpuReadback.h"

#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/Texture.h"

namespace Timefall::RHI
{
	GpuReadback::GpuReadback(const char* debugName)
		: m_DebugName(debugName)
	{}

	bool GpuReadback::Request(CommandList& cmd, Texture& src, const TextureRegion& region)
	{
		const uint64_t frame = RenderDevice::Get().GetFrameValue();
		if (frame == 0 || !src.IsValid())
			return false;

		Slot& slot = m_Slots[frame % FRAMES_IN_FLIGHT];
		if (slot.FrameValue == frame)
			return false;

		const std::optional<TextureRect> rect = ResolveRegion(region, src.GetWidth(), src.GetHeight());
		if (!rect)
			return false;

		const uint64_t bytes = (uint64_t)rect->Width * rect->Height * BytesPerPixel(src.GetPixelFormat());
		if (bytes == 0)
			return false;

		if (!slot.Buffer || slot.Buffer->Size() < bytes)
		{
			slot.Buffer = GpuBuffer::Create({.Size = bytes, .Usage = BufferUsage::TransferDst, .Memory = MemoryType::HostVisible, .DebugName = m_DebugName});
			if (!slot.Buffer->IsValid())
			{
				slot = {};
				return false;
			}
		}

		if (!cmd.CopyTextureToBuffer(src, *slot.Buffer, region))
			return false;

		slot.FrameValue = frame;
		slot.Width = rect->Width;
		slot.Height = rect->Height;
		slot.PixelFormat = src.GetPixelFormat();
		return true;
	}

	std::optional<ReadbackResult> GpuReadback::Poll()
	{
		const uint64_t completed = RenderDevice::Get().GetCompletedFrameValue();

		const Slot* newest = nullptr;
		for (const Slot& slot : m_Slots)
		{
			if (slot.FrameValue != 0 && slot.FrameValue <= completed && (!newest || slot.FrameValue > newest->FrameValue))
				newest = &slot;
		}

		if (!newest)
			return std::nullopt;

		const ReadbackResult result{.Bytes = {static_cast<const std::byte*>(newest->Buffer->GetMapped()),
										(size_t)newest->Width * newest->Height * BytesPerPixel(newest->PixelFormat)}, .Width = newest->Width, .Height = newest->Height, .PixelFormat = newest->PixelFormat};

		const uint64_t newestValue = newest->FrameValue;
		for (Slot& slot : m_Slots)
		{
			if (slot.FrameValue != 0 && slot.FrameValue <= newestValue)
				slot.FrameValue = 0;
		}

		return result;
	}

	bool GpuReadback::IsPending() const
	{
		for (const Slot& slot : m_Slots)
		{
			if (slot.FrameValue != 0)
				return true;
		}

		return false;
	}
}
