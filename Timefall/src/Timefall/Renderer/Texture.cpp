#include "tfpch.h"

#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/Renderer.h"

#include "Platform/OpenGL/OpenGLTexture.h"

namespace Timefall
{
	Ref<Texture2D> Texture2D::Create(const TextureSpecification& spec, Buffer data)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return CreateRef<OpenGLTexture2D>(spec, data);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}