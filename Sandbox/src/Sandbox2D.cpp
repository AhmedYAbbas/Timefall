#include "Sandbox2D.h"

#include <imgui/imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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

Sandbox2D::Sandbox2D()
	: Layer("Sandbox2D"), m_CameraController(1280.0f / 720.0f), m_QuadColor({0.2f, 0.2f, 0.8f, 1.0f})
{
}

void Sandbox2D::OnAttach()
{
	TF_PROFILE_FUNCTION();

	m_CheckerboardTexture = Timefall::Texture2D::Create("assets/textures/Checkerboard.png");
	m_SpriteSheet = Timefall::Texture2D::Create("assets/Game/Textures/tilemap_packed.png");

	m_LadderTexture = Timefall::SubTexture2D::Create(m_SpriteSheet, {11, 4}, {16, 16});
	m_CactusTexture = Timefall::SubTexture2D::Create(m_SpriteSheet, {15, 6}, {16, 16});
	m_ForestTexture = Timefall::SubTexture2D::Create(m_SpriteSheet, {10, 5}, {16, 16}, {3, 3});

	m_MapWidth = s_MapWidth;
	m_MapHeight = strlen(s_TileMap) / m_MapWidth;

	m_TileMap['G'] = Timefall::SubTexture2D::Create(m_SpriteSheet, {2, 6}, {16, 16});
	m_TileMap['R'] = Timefall::SubTexture2D::Create(m_SpriteSheet, {2, 2}, {16, 16});

	m_CameraController.SetZoomLevel(5.0f);
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

#if 0
	{
		TF_PROFILE_SCOPE("Renderer Draw");

		static float rotation = 0.0f;
		rotation += ts * 50.0f;

		Timefall::Renderer2D::BeginScene(m_CameraController.GetCamera());

		Timefall::Renderer2D::DrawQuad(m_CheckerboardTexture, {0.0f, 0.0f, -0.1f}, 0.0f, {20.0f, 20.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, 10.0f);
		Timefall::Renderer2D::DrawQuad(m_CheckerboardTexture, {-2.0f, 0.0f, 0.0f}, rotation, {1.0f, 1.0f}, {0.8f, 0.8f, 1.0f, 1.0f}, 10.0f);
		Timefall::Renderer2D::DrawQuad({0.0f, 0.0f, 0.0f}, 45.0f, {1.0f, 1.0f}, {0.8f, 0.2f, 0.3f, 1.0f});
		Timefall::Renderer2D::DrawQuad({1.0f, 1.0f, 0.0f}, 45.0f, {2.0f, 0.5f}, {0.2f, 0.2f, 0.8f, 1.0f});
		Timefall::Renderer2D::DrawQuad({-3.0f, -1.0f, 0.0f}, 0.0f, {2.0f, 0.5f}, m_QuadColor);
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
#endif

	Timefall::Renderer2D::BeginScene(m_CameraController.GetCamera());

	for (uint32_t y = 0; y < m_MapHeight; ++y)
	{
		for (uint32_t x = 0; x < m_MapWidth; ++x)
		{
			char tileType = s_TileMap[x + y * m_MapWidth];
			Timefall::Ref<Timefall::SubTexture2D> texture;
			if (m_TileMap.find(tileType) != m_TileMap.end())
				texture = m_TileMap[tileType];
			else
				texture = m_CactusTexture;

			Timefall::Renderer2D::DrawQuad(texture, {x - m_MapWidth / 2.0f, m_MapHeight - y - m_MapHeight / 2.0f, 0.0f});
		}
	}

	/*Timefall::Renderer2D::DrawQuad(m_LadderTexture, {0.0f, 0.0f, 0.0f});
	Timefall::Renderer2D::DrawQuad(m_CactusTexture, {1.0f, 0.0f, 0.0f});
	Timefall::Renderer2D::DrawQuad(m_ForestTexture, {-2.0f, 0.0f, 0.0f}, 0.0f, {3, 3});*/

	Timefall::Renderer2D::EndScene();
}

void Sandbox2D::OnEvent(Timefall::Event& e)
{
	TF_PROFILE_FUNCTION();

	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
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
	            Timefall::Application::Get().Shutdown();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

	ImGui::Begin("Stats");

	auto stats = Timefall::Renderer2D::GetStats();
	ImGui::Text("Renderer2D Stats:");
	ImGui::Text("Draw Calls: %d", stats.DrawCalls);
	ImGui::Text("Quads: %d", stats.QuadCount);
	ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
	ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

	ImGui::Separator();
	ImGui::ColorEdit4("Square Color", glm::value_ptr(m_QuadColor));

	uint32_t textureID = m_CheckerboardTexture->GetRendererID();
	ImGui::Image((void*)textureID, ImVec2(256.0f, 256.0f));

	ImGui::End();
	
    ImGui::End();
}
