#include "tfpch.h"

#include "Timefall/Renderer/VertexArray.h"
#include "Timefall/Renderer/Renderer.h"

#include "Platform\OpenGL\OpenGLVertexArray.h"

namespace Timefall
{
	Ref<VertexArray> VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLVertexArray>();
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}