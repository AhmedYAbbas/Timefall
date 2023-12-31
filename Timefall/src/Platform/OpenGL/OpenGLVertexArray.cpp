#include "tfpch.h"
#include "OpenGLVertexArray.h"

#include <glad\glad.h>

namespace Timefall
{
	static GLenum ShaderDataTypeToGLBaseType(ShaderDataType type)
	{
		switch (type)
		{
			case Timefall::ShaderDataType::Float:    return GL_FLOAT;
			case Timefall::ShaderDataType::Float2:   return GL_FLOAT;
			case Timefall::ShaderDataType::Float3:   return GL_FLOAT;
			case Timefall::ShaderDataType::Float4:   return GL_FLOAT;
			case Timefall::ShaderDataType::Mat3:     return GL_FLOAT;
			case Timefall::ShaderDataType::Mat4:     return GL_FLOAT;
			case Timefall::ShaderDataType::Int:	     return GL_INT;
			case Timefall::ShaderDataType::Int2:     return GL_INT;
			case Timefall::ShaderDataType::Int3:     return GL_INT;
			case Timefall::ShaderDataType::Int4:     return GL_INT;
			case Timefall::ShaderDataType::Bool:     return GL_BOOL;
		}

		TF_CORE_ASSERT(false, "Unknown ShaderDataType!");
		return 0;
	}

	OpenGLVertexArray::OpenGLVertexArray()
	{
		glCreateVertexArrays(1, &m_RendererID);
	}

	OpenGLVertexArray::~OpenGLVertexArray()
	{
		glDeleteVertexArrays(1, &m_RendererID);
	}

	void OpenGLVertexArray::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void OpenGLVertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void OpenGLVertexArray::AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer)
	{
		TF_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

		const BufferLayout& layout = vertexBuffer->GetLayout();
		for (const BufferElement& element : layout)
		{
			glEnableVertexAttribArray(m_VertexBufferIndex);
			glVertexAttribPointer(m_VertexBufferIndex, 
				element.GetComponentCount(), 
				ShaderDataTypeToGLBaseType(element.Type), 
				element.Normalized ? GL_TRUE : GL_FALSE, 
				layout.GetStride(), 
				(const void*)(intptr_t)element.Offset);

			m_VertexBufferIndex++;
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void OpenGLVertexArray::SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer)
	{
		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}
}