#include "tfpch.h"

#include "Timefall/Renderer/Renderer2D.h"

#include "Timefall/Renderer/Buffer.h"
#include "Timefall/Renderer/Shader.h"

#include "Timefall/Asset/AssetManager.h"

#include "Timefall/Renderer/MSDFData.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Timefall
{
	struct QuadVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;
		float TexIndex;
		float TilingFactor;

		// Editor Only
		int EntityID;
	};

	struct CircleVertex
	{
		glm::vec3 WorldPosition;
		glm::vec2 LocalPosition;
		glm::vec4 Color;
		float Thickness;
		float Fade;

		// Editor Only
		int EntityID;
	};

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;

		// Editor Only
		int EntityID;
	};

	struct TextVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
		glm::vec2 TexCoords;

		// TODO: bg color for outline/bg

		// Editor Only
		int EntityID;
	};

	struct Renderer2DData
	{
		static constexpr uint32_t MaxQuads = 20000;
		static constexpr uint32_t MaxVertices = MaxQuads * 4;
		static constexpr uint32_t MaxIndices = MaxQuads * 6;
		static constexpr uint32_t MaxTextureSlots = 32;

		// Quads
		Ref<Shader> QuadShader;
		Ref<Texture2D> WhiteTexture;

		uint32_t QuadIndexCount = 0;
		QuadVertex* QuadVertexBufferBase = nullptr;
		QuadVertex* QuadVertexBufferPtr = nullptr;

		// Circles
		Ref<Shader> CircleShader;

		uint32_t CircleIndexCount = 0;
		CircleVertex* CircleVertexBufferBase = nullptr;
		CircleVertex* CircleVertexBufferPtr = nullptr;

		// Lines
		Ref<Shader> LineShader;

		uint32_t LineVertexCount = 0;
		LineVertex* LineVertexBufferBase = nullptr;
		LineVertex* LineVertexBufferPtr = nullptr;

		float LineWidth = 2.0f;

		// Texts
		//Ref<VertexBuffer> TextVertexBuffer;
		Ref<Shader> TextShader;

		uint32_t TextIndexCount = 0;
		TextVertex* TextVertexBufferBase = nullptr;
		TextVertex* TextVertexBufferPtr = nullptr;

		std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
		uint32_t TextureSlotIndex = 1;

		Ref<Texture2D> FontAtlasTexture;

		glm::vec4 QuadVertexPositions[4];

		Renderer2D::Statistics Stats;
	};

	static Renderer2DData s_Data;

	void Renderer2D::Init() {}

	void Renderer2D::Shutdown() {}

	void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform) {}

	void Renderer2D::BeginScene(const OrthographicCamera& camera) {}

	void Renderer2D::BeginScene(const EditorCamera& camera) {}

	void Renderer2D::EndScene() {}

	void Renderer2D::Flush() {}

	void Renderer2D::FlushAndReset() {}

	void Renderer2D::StartBatch() {}

	void Renderer2D::DrawQuad(const glm::mat4& transform, const glm::vec4& color) {}

	void Renderer2D::DrawQuad(const glm::vec2& position, float rotation, const glm::vec2& size, const glm::vec4& color) {}

	void Renderer2D::DrawQuad(const glm::vec3& position, float rotation, const glm::vec2& size, const glm::vec4& color) {}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::vec2& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::mat4& transform, const glm::vec4& tintColor, float tiling) {}

	void Renderer2D::DrawQuad(const Ref<Texture2D>& texture, const glm::vec3& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{}

	void Renderer2D::DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec2& position, float rotation, const glm::vec2& size,
		const glm::vec4& tintColor, float tiling)
	{}

	void Renderer2D::DrawQuad(const Ref<SubTexture2D>& subTexture, const glm::vec3& position, float rotation, const glm::vec2& size,
		const glm::vec4& tint, float tilingFactor)
	{}

	void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int entityID) {}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawRect(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawRect(const glm::mat4& transform, const glm::vec4& color, int entityID) {}

	void Renderer2D::DrawSprite(const glm::mat4& transform, SpriteRendererComponent src, int entityID) {}

	void Renderer2D::DrawString(
		const std::string& text, const Ref<Font>& font, const glm::mat4& transform, const TextParams& params, int entityID)
	{}

	void Renderer2D::DrawString(const std::string& text, const glm::mat4& transform, const TextComponent& component, int entityID) {}

	float Renderer2D::GetLineWidth()
	{
		return 0.0f;
	}

	void Renderer2D::SetLineWidth(float width) {}

	Renderer2D::Statistics& Renderer2D::GetStats()
	{
		static Statistics s_Empty{};
		return s_Empty;
	}

	void Renderer2D::ResetStats() {}
}
