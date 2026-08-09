#include "tfpch.h"
#include "Timefall/Renderer/Buffer.h"

namespace Timefall
{
	namespace
	{
		class StubVertexBuffer : public VertexBuffer
		{
		public:
			void Bind() const override {}
			void Unbind() const override {}
			void SetData(const void*, uint32_t) override {}

			void SetLayout(const BufferLayout& layout) override { m_Layout = layout; }
			const BufferLayout& GetLayout() const override { return m_Layout; }

		private:
			BufferLayout m_Layout;
		};

		class StubIndexBuffer : public IndexBuffer
		{
		public:
			explicit StubIndexBuffer(uint32_t count)
				: m_Count(count)
			{}

			void Bind() const override {}
			void Unbind() const override {}

			uint32_t GetCount() const override { return m_Count; }

		private:
			uint32_t m_Count;
		};
	}

	Ref<VertexBuffer> VertexBuffer::Create(uint32_t)
	{
		return CreateRef<StubVertexBuffer>();
	}

	Ref<VertexBuffer> VertexBuffer::Create(float*, uint32_t)
	{
		return CreateRef<StubVertexBuffer>();
	}

	Ref<IndexBuffer> IndexBuffer::Create(uint32_t*, uint32_t count)
	{
		return CreateRef<StubIndexBuffer>(count);
	}
}
