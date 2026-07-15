#include "tfpch.h"

#include "Platform/OpenGL/OpenGLUniformBuffer.h"
#include "Platform/OpenGL/GPUMemoryTracker.h"

#include <glad/glad.h>

namespace Timefall
{
	OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size, uint32_t binding)
	{
		glCreateBuffers(1, &m_RendererID);
		glNamedBufferData(m_RendererID, size, nullptr, GL_DYNAMIC_DRAW);
		GPUMemoryTracker::Track(GPUMemCategory::Buffers, m_RendererID, size);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
	}

	OpenGLUniformBuffer::~OpenGLUniformBuffer()
	{
		GPUMemoryTracker::Untrack(GPUMemCategory::Buffers, m_RendererID);
		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glNamedBufferSubData(m_RendererID, offset, size, data);
	}
}
