#include "tfpch.h"
#include "Timefall\Renderer\RenderCommand.h"
#include "Platform\OpenGL\OpenGLRendererAPI.h"

namespace Timefall
{
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI();
}