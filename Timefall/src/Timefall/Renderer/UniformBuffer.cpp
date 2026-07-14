#include "tfpch.h"

#include "Timefall/Renderer/UniformBuffer.h"
#include "Timefall/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLUniformBuffer.h"

namespace Timefall
{
	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None: TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLUniformBuffer>(size, binding);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
