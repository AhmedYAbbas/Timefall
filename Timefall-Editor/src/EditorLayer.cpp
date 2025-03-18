#include "EditorLayer.h"

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Timefall
{
	static const uint32_t s_MapWidth = 24;
	static const char* s_TileMap =
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGGRRRRRRRRRRRRR"
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGGRGGGGGGGGGGGG"
		"GGGGGGGGGGRGGGGGGGGGGGGG"
		"GGGGGGGGGRGGGGGGGGGGGGGG"
		"GGGGGGGGRGGGGGGGGGGGGGGG"
		"GGGGGGGRGGGGGGGGGGGGGGGG"
		"GGGGGGRGGGGGGGGGGGGGGGGG"
		"RRRRRRGGGGGGGGGGGGGGGGGG"
		"GGGGGGGGGGGGGGGGGGGGGGGG"
		"RRRRRRRRRRRRRRRRRRRRRRRR";

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"), m_CameraController(1280.0f / 720.0f), m_QuadColor({ 0.2f, 0.2f, 0.8f, 1.0f })
	{
	}

	void EditorLayer::OnAttach()
	{
		TF_PROFILE_FUNCTION();

		FramebufferSpecification fbSpec;
		fbSpec.Width = 1280;
		fbSpec.Height = 720;
		m_Framebuffer = Framebuffer::Create(fbSpec);

		m_CheckerboardTexture = Texture2D::Create("assets/textures/Checkerboard.png");
		m_SpriteSheet = Texture2D::Create("assets/Game/Textures/tilemap_packed.png");

		m_LadderTexture = SubTexture2D::Create(m_SpriteSheet, { 11, 4 }, { 16, 16 });
		m_CactusTexture = SubTexture2D::Create(m_SpriteSheet, { 15, 6 }, { 16, 16 });
		m_ForestTexture = SubTexture2D::Create(m_SpriteSheet, { 10, 5 }, { 16, 16 }, { 3, 3 });

		m_MapWidth = s_MapWidth;
		m_MapHeight = (uint32_t)strlen(s_TileMap) / m_MapWidth;

		m_TileMap['G'] = SubTexture2D::Create(m_SpriteSheet, { 2, 6 }, { 16, 16 });
		m_TileMap['R'] = SubTexture2D::Create(m_SpriteSheet, { 2, 2 }, { 16, 16 });

		m_CameraController.SetZoomLevel(5.0f);
	}

	void EditorLayer::OnDetach()
	{
		TF_PROFILE_FUNCTION();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TF_PROFILE_FUNCTION();

		// Update
		if (m_ViewportFocused)
			m_CameraController.OnUpdate(ts);

		// Render
		Renderer2D::ResetStats();
		{
			TF_PROFILE_SCOPE("Renderer Prep");
			m_Framebuffer->Bind();
			RenderCommand::Clear({ 0.1f, 0.1f, 0.1f, 1.0f });
		}


		{
			TF_PROFILE_SCOPE("Renderer Draw");

			static float rotation = 0.0f;
			rotation += ts * 50.0f;

			Renderer2D::BeginScene(m_CameraController.GetCamera());

			Renderer2D::DrawQuad(m_CheckerboardTexture, { 0.0f, 0.0f, -0.1f }, 0.0f, { 20.0f, 20.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, 10.0f);
			Renderer2D::DrawQuad(m_CheckerboardTexture, { -2.0f, 0.0f, 0.0f }, rotation, { 1.0f, 1.0f }, { 0.8f, 0.8f, 1.0f, 1.0f }, 10.0f);
			Renderer2D::DrawQuad({ 0.0f, 0.0f, 0.0f }, 45.0f, { 1.0f, 1.0f }, { 0.8f, 0.2f, 0.3f, 1.0f });
			Renderer2D::DrawQuad({ 1.0f, 1.0f, 0.0f }, 45.0f, { 2.0f, 0.5f }, { 0.2f, 0.2f, 0.8f, 1.0f });
			Renderer2D::DrawQuad({ -3.0f, -1.0f, 0.0f }, 0.0f, { 2.0f, 0.5f }, m_QuadColor);
			Renderer2D::DrawQuad({ 1.0f, 0.5f, 0.0f }, 0.0f, { 1.0f, 1.0f }, { 1.0f, 1.0f, 0.0f, 1.0f });

			for (float y = -5.0f; y < 5.0f; y += 0.5f)
			{
				for (float x = -5.0f; x < 5.0f; x += 0.5f)
				{
					glm::vec4 color = { (x + 5.0f) / 10.f, 0.4f, (y + 5.0f) / 10.f, 0.8f };
					Renderer2D::DrawQuad({ x, y }, 0.0f, { 0.45f, 0.45f }, color);
				}
			}

			Renderer2D::EndScene();
			m_Framebuffer->Unbind();
		}

#if 0
		Renderer2D::BeginScene(m_CameraController.GetCamera());

		for (uint32_t y = 0; y < m_MapHeight; ++y)
		{
			for (uint32_t x = 0; x < m_MapWidth; ++x)
			{
				char tileType = s_TileMap[x + y * m_MapWidth];
				Ref<SubTexture2D> texture;
				if (m_TileMap.find(tileType) != m_TileMap.end())
					texture = m_TileMap[tileType];
				else
					texture = m_CactusTexture;

				Renderer2D::DrawQuad(texture, { x - m_MapWidth / 2.0f, m_MapHeight - y - m_MapHeight / 2.0f, 0.0f });
			}
		}

		/*Renderer2D::DrawQuad(m_LadderTexture, {0.0f, 0.0f, 0.0f});
		Renderer2D::DrawQuad(m_CactusTexture, {1.0f, 0.0f, 0.0f});
		Renderer2D::DrawQuad(m_ForestTexture, {-2.0f, 0.0f, 0.0f}, 0.0f, {3, 3});*/

		Renderer2D::EndScene();
#endif
	}

	void EditorLayer::OnEvent(Event& e)
	{
		TF_PROFILE_FUNCTION();

		m_CameraController.OnEvent(e);
	}

	void EditorLayer::OnImGuiRender()
	{
		static bool opt_fullscreen = true;
		static bool opt_padding = false;
		static bool dockspaceOpen = true;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
		{
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
		}

		// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
		// and handle the pass-thru hole, so we ask Begin() to not render a background.
		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
		// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
		// all active windows docked into it will lose their parent and become undocked.
		// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
		// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// Submit the DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit", NULL, false))
				{
					dockspaceOpen = false;
					Application::Get().Shutdown();
				}
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		ImGui::Begin("Stats");

		auto stats = Renderer2D::GetStats();
		ImGui::Text("Renderer2D Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

		ImGui::Separator();
		ImGui::ColorEdit4("Square Color", glm::value_ptr(m_QuadColor));
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Viewport");

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();

		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportFocused || !m_ViewportHovered);

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		if (m_ViewportSize != *((glm::vec2*)&viewportSize) && viewportSize.x > 0 && viewportSize.y > 0)
		{
			m_ViewportSize = { viewportSize.x, viewportSize.y };
			m_Framebuffer->Resize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);
			m_CameraController.OnResize(viewportSize.x, viewportSize.y);
		}

		uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImGui::Image((void*)textureID, viewportSize, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
		ImGui::End();
		ImGui::PopStyleVar();

		ImGui::End();
	}

}