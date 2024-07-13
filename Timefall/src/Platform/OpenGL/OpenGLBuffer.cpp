#include "tfpch.h"

#include "Platform/OpenGL/OpenGLBuffer.h"

#include <glad/glad.h>

namespace Timefall
{
	/////////////////////////////////////////////////////////////////////////////////
	/// VertexBuffer ////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////

	OpenGLVertexBuffer::OpenGLVertexBuffer()
	{
		TF_PROFILE_FUNCTION();

		glCreateBuffers(1, &m_RendererID);
	}

	OpenGLVertexBuffer::~OpenGLVertexBuffer()
	{
		TF_PROFILE_FUNCTION();

		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLVertexBuffer::SetData(float* vertices, uint32_t size)
	{
		TF_PROFILE_FUNCTION();

		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	void OpenGLVertexBuffer::Bind() const
	{
		TF_PROFILE_FUNCTION();

		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLVertexBuffer::Unbind() const
	{
		TF_PROFILE_FUNCTION();

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	/////////////////////////////////////////////////////////////////////////////////
	/// IndexBuffer /////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////////

	OpenGLIndexBuffer::OpenGLIndexBuffer()
		: m_Count(0)
	{
		TF_PROFILE_FUNCTION();

		glCreateBuffers(1, &m_RendererID);
	}

	OpenGLIndexBuffer::~OpenGLIndexBuffer()
	{
		TF_PROFILE_FUNCTION();

		glDeleteBuffers(1, &m_RendererID);
	}

	void OpenGLIndexBuffer::SetData(uint32_t* indices, uint32_t count)
	{
		TF_PROFILE_FUNCTION();

		m_Count = count;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	void OpenGLIndexBuffer::Bind() const
	{
		TF_PROFILE_FUNCTION();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void OpenGLIndexBuffer::Unbind() const
	{
		TF_PROFILE_FUNCTION();

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
}