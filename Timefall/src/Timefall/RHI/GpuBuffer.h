#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/RHI/RHITypes.h"

#include <cstdint>

namespace Timefall::RHI
{
	enum class BufferUsage : uint8_t
	{
		None		= 0,
		Vertex		= BIT(0),
		Index		= BIT(1),
		Uniform		= BIT(2),
		Storage		= BIT(3),
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

	enum  class MemoryType : uint8_t
	{
		DeviceLocal = 0,
		HostWrite
	};

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

		~GpuBuffer();

		GpuBuffer(const GpuBuffer&) = delete;
		GpuBuffer& operator=(const GpuBuffer&) = delete;

		bool Write(const void* data, uint64_t size, uint64_t offset = 0);

		void* GetMapped() const;

		uint64_t Size() const;
		bool IsValid() const;

	private:
		GpuBuffer() = default;

		friend class CommandList;

		struct Impl;
		Impl* m_Impl = nullptr;
	};
}
