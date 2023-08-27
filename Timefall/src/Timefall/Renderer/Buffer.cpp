#include "tfpch.h"
#include "Buffer.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Timefall
{
	VertexBuffer* VertexBuffer::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::OpenGL: return new OpenGLVertexBuffer();
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	IndexBuffer* IndexBuffer::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::OpenGL: return new OpenGLIndexBuffer();
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
