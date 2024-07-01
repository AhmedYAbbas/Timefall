#include "tfpch.h"
#include "Timefall/Renderer/RenderCommand.h"

namespace Timefall
{
	Scope<RendererAPI> RenderCommand::s_RendererAPI = RendererAPI::Create();
}