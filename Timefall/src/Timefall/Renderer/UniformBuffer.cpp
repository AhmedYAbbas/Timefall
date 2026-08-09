#include "tfpch.h"

#include "Timefall/Renderer/UniformBuffer.h"

namespace Timefall
{
	namespace
	{
		class StubUniformBuffer : public UniformBuffer
		{
		public:
			void SetData(const void*, uint32_t, uint32_t) override {}
		};
	}

	Ref<UniformBuffer> UniformBuffer::Create(uint32_t, uint32_t)
	{
		return CreateRef<StubUniformBuffer>();
	}
}
