#pragma once

#include "Timefall/Renderer/Buffer.h"

namespace Timefall
{
	class OpenGLVertexBuffer : public VertexBuffer
	{
	public:
		OpenGLVertexBuffer();
		~OpenGLVertexBuffer() override;

		void SetData(float* vertices, uint32_t size) override;
		void Bind() const override;
		void Unbind() const override;

	private:
		uint32_t m_RendererID;
	};

	class OpenGLIndexBuffer : public IndexBuffer
	{
	public:
		OpenGLIndexBuffer();
		~OpenGLIndexBuffer() override;

		void SetData(uint32_t* indices, uint32_t count) override;
		void Bind() const override;
		void Unbind() const override;

		uint32_t GetCount() const override { return m_Count; }

	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	};
}