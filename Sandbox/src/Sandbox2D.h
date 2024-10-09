#pragma once

#include "Timefall.h"

class Sandbox2D : public Timefall::Layer
{
public:
	Sandbox2D();
	virtual ~Sandbox2D() = default;

	virtual void OnAttach() override;
	virtual void OnDetach() override;

	virtual void OnUpdate(Timefall::Timestep ts) override;
	virtual void OnEvent(Timefall::Event& e) override;
	virtual void OnImGuiRender() override;
private:
	Timefall::OrthographicCameraController m_CameraController;

	// Temp
	Timefall::Ref<Timefall::Shader> m_FlatColorShader;
	Timefall::Ref<Timefall::VertexArray> m_SquareVA;

	Timefall::Ref<Timefall::Texture2D> m_CheckerboardTexture;
	Timefall::Ref<Timefall::Texture2D> m_SpriteSheet;
	Timefall::Ref<Timefall::SubTexture2D> m_LadderTexture, m_CactusTexture, m_ForestTexture;

	uint32_t m_MapWidth, m_MapHeight;
	std::unordered_map<char, Timefall::Ref<Timefall::SubTexture2D>> m_TileMap;

	glm::vec4 m_QuadColor = {0.2f, 0.3f, 0.8f, 1.0f};
};