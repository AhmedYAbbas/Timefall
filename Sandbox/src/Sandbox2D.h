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

	glm::vec4 m_SqaureColor = {0.2f, 0.3f, 0.8f, 1.0f};

	struct ProfileResult
	{
		const char* Name;
		float Time;
	};

	std::vector<ProfileResult> m_ProfileResults;
};