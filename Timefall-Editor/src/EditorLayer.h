#pragma once

#include "Timefall.h"

namespace Timefall
{
	class EditorLayer : public Layer
	{
	public:
		EditorLayer();
		virtual ~EditorLayer() = default;

		virtual void OnAttach() override;
		virtual void OnDetach() override;

		virtual void OnUpdate(Timestep ts) override;
		virtual void OnEvent(Event& e) override;
		virtual void OnImGuiRender() override;

	private:
		OrthographicCameraController m_CameraController;

		// Temp
		Ref<Shader> m_FlatColorShader;
		Ref<VertexArray> m_SquareVA;
		Ref<Framebuffer> m_Framebuffer;

		Ref<Texture2D> m_CheckerboardTexture;
		Ref<Texture2D> m_SpriteSheet;
		Ref<SubTexture2D> m_LadderTexture, m_CactusTexture, m_ForestTexture;

		uint32_t m_MapWidth, m_MapHeight;
		std::unordered_map<char, Ref<SubTexture2D>> m_TileMap;

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };

		glm::vec4 m_QuadColor = { 0.2f, 0.3f, 0.8f, 1.0f };
	};
}