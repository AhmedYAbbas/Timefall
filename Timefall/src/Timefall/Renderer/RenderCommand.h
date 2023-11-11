#pragma once

#include "RendererAPI.h"
#include "Timefall/Core.h"

namespace Timefall
{
	class RenderCommand 
	{
	public:
		inline static void Init()
		{
			s_RendererAPI->Init();
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