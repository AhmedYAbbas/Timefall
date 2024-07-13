#pragma once

#include <Timefall.h>

class ExampleLayer : public Timefall::Layer
{
public:
	ExampleLayer();
	virtual ~ExampleLayer() = default;

	void OnAttach() override;
	void OnDetach() override;

	void OnUpdate(Timefall::Timestep ts) override;
	void OnImGuiRender() override;
	void OnEvent(Timefall::Event& e) override;

private:
	Timefall::ShaderLibrary m_ShaderLibrary;
	Timefall::Ref<Timefall::Shader> m_Shader;
	Timefall::Ref<Timefall::VertexArray> m_VertexArray;

	Timefall::Ref<Timefall::Shader> m_FlatColorShader;
	Timefall::Ref<Timefall::VertexArray> m_SquareVA;

	Timefall::Ref<Timefall::Texture2D> m_Texture, m_ChernoLogoTexture;

	Timefall::OrthographicCameraController m_CameraController;
	glm::vec3 m_SquareColor = {0.2f, 0.3f, 0.8f};
};