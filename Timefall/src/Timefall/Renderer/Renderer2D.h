#pragma once

#include "Camera.h"

namespace Timefall
{
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();
		
		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();

		// Primitives
		static void DrawQuad(const glm::vec2& position, float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
		static void DrawQuad(const glm::vec3& position, float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
	};
}