#pragma once

#include "Timefall/Renderer/Camera.h"
#include "Timefall/Renderer/Texture.h"

namespace Timefall
{
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();
		
		static void BeginScene(const OrthographicCamera& camera);
		static void EndScene();
		static void Flush();

		// Primitives
		static void DrawQuad(const glm::vec2& position = glm::vec2(1), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
		static void DrawQuad(const glm::vec3& position = glm::vec3(1), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& color = glm::vec4(1));
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec2& position = glm::vec2(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& tintColor = glm::vec4(1), float tiling = 1.0f);
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec3& position = glm::vec3(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1), const glm::vec4& tintColor = glm::vec4(1), float tiling = 1.0f);

		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};
		static Statistics& GetStats();
		static void ResetStats();

	private:
		static void FlushAndReset();
	};
}