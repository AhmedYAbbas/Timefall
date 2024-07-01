#include "tfpch.h"

#include "Timefall/Renderer/GraphicsContext.h"
#include "Timefall/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLContext.h"

namespace Timefall
{
	Scope<GraphicsContext> Timefall::GraphicsContext::Create(void* window)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:    TF_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); return nullptr;
			case RendererAPI::API::OpenGL:  return CreateScope<OpenGLContext>(static_cast<GLFWwindow*>(window));
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}