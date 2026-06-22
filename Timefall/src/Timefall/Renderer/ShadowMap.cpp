#include "tfpch.h"
#include "Timefall/Renderer/ShadowMap.h"

#include "Timefall/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLShadowMap.h"

namespace Timefall
{
	Ref<ShadowMap> ShadowMap::Create(uint32_t resolution, uint32_t layers)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!");
				return nullptr;
			case RendererAPI::API::OpenGL:
				return CreateRef<OpenGLShadowMap>(resolution, layers);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
