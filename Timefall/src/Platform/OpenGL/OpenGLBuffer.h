#pragma once

#include "Timefall/Renderer/Buffer.h"

namespace Timefall
{
	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer();
		~OpenGLVertexBuffer() override;

		virtual void SetData(float* vertices, uint32_t size) override;
		virtual void Bind() const override;
		virtual void Unbind() const override;

	private:
		uint32_t m_RendererID;
	};

	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer();
		~OpenGLIndexBuffer() override;

		virtual void SetData(uint32_t* indices, uint32_t count) override;
		virtual void Bind() const override;
		virtual void Unbind() const override;

		uint32_t GetCount() const override { return m_Count; }

	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	};
}