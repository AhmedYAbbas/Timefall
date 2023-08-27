#pragma once

namespace Timefall
{
	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() {}

		virtual void SetData(float* vertices, uint32_t size) = 0;
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		static VertexBuffer* Create();
	};

	class IndexBuffer
	{
	public:
		virtual ~IndexBuffer() {}

		virtual void SetData(uint32_t* indices, uint32_t count) = 0;
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;

		virtual uint32_t GetCount() const = 0;

		static IndexBuffer* Create();
	};
}
