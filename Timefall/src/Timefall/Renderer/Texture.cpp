#include "tfpch.h"
#include "Texture.h"

#include "Timefall/Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLTexture.h"

namespace Timefall
{
	Ref<Texture2D> Texture2D::Create(const std::string& path)
	{
		switch (Renderer::GetAPI())
		{
			case RendererAPI::API::None:   TF_CORE_ASSERT(false, "RendererAPI::None is not currently supported!"); return nullptr;
			case RendererAPI::API::OpenGL: return std::make_shared<OpenGLTexture2D>(path);
		}

		TF_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}