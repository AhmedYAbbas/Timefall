#include "Sandbox2D.h"

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f)
{
}

void Sandbox2D::OnAttach()
{
	TF_PROFILE_FUNCTION();

	m_CheckerboardTexture = Timefall::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach()
{
	TF_PROFILE_FUNCTION();
}

void Sandbox2D::OnUpdate(Timefall::Timestep ts)
{
	TF_PROFILE_FUNCTION();

	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	Timefall::Renderer2D::ResetStats();
	{
		TF_PROFILE_SCOPE("Renderer Prep");
		Timefall::RenderCommand::Clear({0.1f, 0.1f, 0.1f, 1.0f});
	}

	{
		TF_PROFILE_SCOPE("Renderer Draw");

		static float rotation = 0.0f;
		rotation += ts * 50.0f;

		Timefall::Renderer2D::BeginScene(m_CameraController.GetCamera());

		Timefall::Renderer2D::DrawQuad(m_CheckerboardTexture, {0.0f, 0.0f, -0.1f}, 0.0f, {20.0f, 20.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 10.0f);
		Timefall::Renderer2D::DrawQuad(m_CheckerboardTexture, {-2.0f, 0.0f, 0.0f}, rotation, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 10.0f);
		Timefall::Renderer2D::DrawQuad({0.0f, 0.0f, 0.0f}, 45.0f, {1.0f, 1.0f}, {0.8f, 0.2f, 0.3f, 1.0f});
		Timefall::Renderer2D::DrawQuad({1.0f, 1.0f, 0.0f}, 45.0f, {2.0f, 0.5f}, {0.2f, 0.2f, 0.8f, 1.0f});
		Timefall::Renderer2D::DrawQuad({-3.0f, -1.0f, 0.0f}, 0.0f, {2.0f, 0.5f}, {0.2f, 0.2f, 0.8f, 1.0f});
		Timefall::Renderer2D::DrawQuad({1.0f, 0.5f, 0.0f}, 0.0f, {1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f});

		for (float y = -5.0f; y < 5.0f; y += 0.5f)
		{
			for (float x = -5.0f; x < 5.0f; x += 0.5f)
			{
				glm::vec4 color = {(x + 5.0f) / 10.f, 0.4f, (y + 5.0f) / 10.f, 0.8f};
				Timefall::Renderer2D::DrawQuad({x, y}, 0.0f, {0.45f, 0.45f}, color);
			}
		}

		Timefall::Renderer2D::EndScene();
	}
}

void Sandbox2D::OnEvent(Timefall::Event& e)
{
	TF_PROFILE_FUNCTION();

	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{
	ImGui::Begin("Stats");

	auto stats = Timefall::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
	//ImGui::ColorEdit4("Square Color", glm::value_ptr(m_SqaureColor));
	ImGui::End();
}
