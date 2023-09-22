#include "tfpch.h"
#include "VertexArray.h"

#include "Renderer.h"
#include "Platform\OpenGL\OpenGLVertexArray.h"

namespace Timefall
{
	VertexArray* VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::OpenGL: return new OpenGLVertexArray();
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}