#pragma once

#include "Timefall\Renderer\RendererAPI.h"

#include <glm\glm.hpp>

namespace Timefall
{
	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void Clear(const glm::vec4& color) override;
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray) override;
	};
}