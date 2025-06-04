#pragma once

#include "Timefall/Renderer/OrthographicCamera.h"
#include "Timefall/Renderer/Camera.h"
#include "Timefall/Renderer/Texture.h"
#include "Timefall/Renderer/SubTexture2D.h"
#include "Timefall/Renderer/EditorCamera.h"
#include "Timefall/Scene/Components.h"

namespace Timefall
{
	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();
		
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void BeginScene(const OrthographicCamera& camera);
		static void BeginScene(const EditorCamera& camera);
		static void EndScene();
		static void Flush();

		// Primitives
		static void DrawQuad(const glm::mat4& transform = glm::mat4(1.0f), const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec2& position = glm::vec2(1.0f), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const glm::vec3& position = glm::vec3(1.0f), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& color = glm::vec4(1.0f));
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec2& position = glm::vec2(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), float tiling = 1.0f);
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::mat4& transform = glm::mat4(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), float tiling = 1.0f);
		static void DrawQuad(const Ref<Texture2D>& texture, const glm::vec3& position = glm::vec3(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), float tiling = 1.0f);
		static void DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec2& position = glm::vec2(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), float tiling = 1.0f);
		static void DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec3& position = glm::vec3(0), float rotation = 0.0f, const glm::vec2& size = glm::vec2(1.0f), const glm::vec4& tintColor = glm::vec4(1.0f), float tiling = 1.0f);

		static void DrawCircle(const glm::mat4& transform = glm::mat4(1.0f), const glm::vec4& color = glm::vec4(1.0f), float thickness = 1.0f, float fade = 0.005f, int entityID = -1);

		static void DrawSprite(const glm::mat4& transform, SpriteRendererComponent src, int entityID = -1);

		struct Statistics
		{
			uint32_t DrawCalls = 0;
			uint32_t QuadCount = 0;
			uint32_t CircleCount = 0;

			uint32_t GetTotalVertexCount() { return QuadCount * 4; }
			uint32_t GetTotalIndexCount() { return QuadCount * 6; }
		};
		static Statistics& GetStats();
		static void ResetStats();

	private:
		static void FlushAndReset();
		static void StartBatch();
	};
}