#pragma once

#include "Timefall\Renderer\RendererAPI.h"

#include <glm\glm.hpp>

namespace Timefall
{
	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Clear(const glm::vec4& color) override;
		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;
	};
}