#pragma once

#include "Camera.h"
#include "Texture.h"

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
		static void DrawQuad(const glm::vec2& position = glm::vec2(1), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
		static void DrawQuad(const glm::vec3& position = glm::vec3(1), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec2& position = glm::vec2(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& tint = glm::vec4(1));
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec3& position = glm::vec3(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& tint = glm::vec4(1));
	};
}