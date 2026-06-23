#include "tfpch.h"
#include "Timefall/Renderer/CubeShadowMap.h"

#include "Timefall/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLCubeShadowMap.h"

namespace Timefall
{
	Ref<CubeShadowMap> CubeShadowMap::Create(uint32_t resolution, uint32_t cubeCount)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:
				TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!");
				return nullptr;
			case RendererAPI::API::OpenGL:
				return CreateRef<OpenGLCubeShadowMap>(resolution, cubeCount);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
