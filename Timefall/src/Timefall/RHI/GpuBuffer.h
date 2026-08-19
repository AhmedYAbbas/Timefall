#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

#include <cstdint>

namespace Timefall::RHI
{
	enum class BufferUsage : uint8_t {
		None = 0,
		Vertex = BIT(0),
		Index = BIT(1),
		Uniform = BIT(2),
		Storage = BIT(3),
		TransferSrc = BIT(4),
		TransferDst = BIT(5)
	};

	constexpr BufferUsage operator|(BufferUsage a, BufferUsage b)
	{
		return (BufferUsage)((uint8_t)a | (uint8_t)b);
	}

	constexpr bool HasFlag(BufferUsage value, BufferUsage flag)
	{
		return ((uint8_t)value & (uint8_t)flag) != 0;
	}

	// DeviceLocal: no CPU access; fill it through GpuBuffer::CreateWithData.
	// HostWrite: persistently mapped and write-combined. Write sequentially, never read back.
	enum class MemoryType : uint8_t { DeviceLocal = 0, HostWrite };

	struct GpuBufferDesc
	{
		uint64_t Size = 0;
		BufferUsage Usage = BufferUsage::None;
		MemoryType Memory = MemoryType::DeviceLocal;
		const char* DebugName = nullptr;
	};

	class TF_API GpuBuffer
	{
	public:
		static Ref<GpuBuffer> Create(const GpuBufferDesc& desc);

		// DeviceLocal only. Allocates and uploads `desc.Size` bytes through a staging copy;
		// TransferDst is added to the usage automatically. Outside an UploadScope this blocks until
		// the copy completes; inside one, the bytes are not on the GPU - and the buffer must not be
		// bound - until the scope closes.
		static Ref<GpuBuffer> CreateWithData(const GpuBufferDesc& desc, const void* data);

		~GpuBuffer();

		GpuBuffer(const GpuBuffer&) = delete;
		GpuBuffer& operator=(const GpuBuffer&) = delete;

		// HostWrite only. Returns false (and logs) on a DeviceLocal buffer or an out-of-range write.
		bool Write(const void* data, uint64_t size, uint64_t offset = 0);

		// nullptr for DeviceLocal. Valid for the buffer's whole lifetime - the mapping is persistent.
		void* GetMapped() const;

		void* GetNativeHandle() const;

		uint64_t Size() const;
		bool IsValid() const;

	private:
		GpuBuffer() = default;

		friend class CommandList;

		struct Impl;
		Impl* m_Impl = nullptr;
	};

	// Coalesces every upload made during its lifetime into one command buffer and one GPU
	// round-trip. Nesting is refcounted; only the outermost scope submits.
	class TF_API UploadScope
	{
	public:
		UploadScope();
		~UploadScope();

		UploadScope(const UploadScope&) = delete;
		UploadScope& operator=(const UploadScope&) = delete;
	};
}
