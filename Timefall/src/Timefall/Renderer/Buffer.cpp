#include "tfpch.h"
#include "Timefall/Renderer/Buffer.h"

#include "Timefall/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Timefall
{
	Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(size);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexBuffer>(vertices, size);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}

	Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLIndexBuffer>(indices, count);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
