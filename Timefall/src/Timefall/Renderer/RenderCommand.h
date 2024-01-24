#pragma once

#include "Timefall/Renderer/RendererAPI.h"
#include "Timefall/Core/Core.h"

namespace Timefall
{
	class RenderCommand 
	{
	public:
		inline static void Init()
		{
			s_RendererAPI->Init();
		}

		inline static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
		{
			s_RendererAPI->SetViewport(x, y, width, height);
		}

		inline static void Clear(const glm::vec4& color) 
		{
			s_RendererAPI->Clear(color);
		}

		inline static void DrawIndexed(const Ref<VertexArray>& vertexArray) 
		{
			s_RendererAPI->DrawIndexed(vertexArray);
		}
		
	private:
		static Scope<RendererAPI> s_RendererAPI;
	};
}