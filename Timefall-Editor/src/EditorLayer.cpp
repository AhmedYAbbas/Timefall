#include "EditorLayer.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/SceneManager.h"

#include "Timefall/Utils/PlatformUtils.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Renderer/Font.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Timefall/Renderer/ShaderLibrary.h"
#include "Timefall/Asset/TextureImporter.h"
#include "Timefall/Asset/SceneImporter.h"
#include "Timefall/Asset/AssetManager.h"
#include "Timefall/Asset/EditorAssetManager.h"

#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/CommandList.h"
#include "Timefall/RHI/FrameAllocator.h"
#include "Timefall/RHI/Bindings.h"

#include "Timefall/ImGui/ImGuiTextures.h"

#include <imgui/imgui.h>
#include "ImGuizmo.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>

namespace Timefall
{
	static Ref<Font> s_Font;

	EditorLayer::EditorLayer()
		: Layer("EditorLayer"),
		  m_CameraController(1280.0f / 720.0f)
	{
		s_Font = Font::GetDefault();
	}

	void EditorLayer::OnAttach()
	{
		TF_PROFILE_FUNCTION();

		m_PlayIcon = TextureImporter::LoadTexture2D("Resources/Icons/PlayButton.png");
		m_PauseIcon = TextureImporter::LoadTexture2D("Resources/Icons/PauseButton.png");
		m_SimulateIcon = TextureImporter::LoadTexture2D("Resources/Icons/SimulateButton.png");
		m_StepIcon = TextureImporter::LoadTexture2D("Resources/Icons/StepButton.png");
		m_StopIcon = TextureImporter::LoadTexture2D("Resources/Icons/StopButton.png");

		m_ViewportTarget = RHI::RenderTarget::Create({.Width = 1280,
			.Height = 720,
			.ColorFormat = {RHI::Format::RGBA8Unorm, RHI::Format::R32I},
			.ColorCount = 2,
			.DepthFormat = RHI::Format::D32F,
			.DebugName = "ViewportTarget"});

		m_TriangleShader = ShaderLibrary::Load("Assets/Shaders/Triangle.slang");

		RHI::GraphicsPipelineDesc triangleDesc;
		triangleDesc.ShaderModule = m_TriangleShader;
		triangleDesc.ColorFormats[0] = RHI::Format::RGBA8Unorm;
		triangleDesc.ColorFormats[1] = RHI::Format::R32I;
		triangleDesc.ColorCount = 2;
		triangleDesc.DepthFormat = RHI::Format::D32F;
		triangleDesc.Depth = {.Test = true, .Write = true};
		triangleDesc.Raster.Cull = RHI::CullMode::None;
		triangleDesc.DebugName = "TrianglePipeline";
		triangleDesc.VertexLayout = {{ShaderDataType::Float3, "a_Position"}, {ShaderDataType::Float3, "a_Color"}};
		m_TrianglePipeline = RHI::GraphicsPipeline::Create(triangleDesc);

		struct BringUpVertex
		{
			glm::vec3 Position;
			glm::vec3 Color;
		};

		constexpr BringUpVertex triangleVertices[]{
			{{-0.9f, 0.6f, 0.0f}, {1.0f, 0.0f, 0.0f}},
			{{-0.1f, 0.6f, 0.0f}, {0.0f, 1.0f, 0.0f}},
			{{-0.5f, -0.6f, 0.0f}, {0.0f, 0.0f, 1.0f}},
		};
		constexpr uint32_t triangleIndices[]{0, 1, 2};

		{
			// Both buffers in one command buffer and one round-trip. The braces are load-bearing:
			// neither buffer holds its data until the scope closes.
			RHI::UploadScope upload;

			m_TriangleVertexBuffer = RHI::GpuBuffer::CreateWithData(
				{.Size = sizeof(triangleVertices), .Usage = RHI::BufferUsage::Vertex, .DebugName = "TriangleVertices"}, triangleVertices);

			m_TriangleIndexBuffer = RHI::GpuBuffer::CreateWithData(
				{.Size = sizeof(triangleIndices), .Usage = RHI::BufferUsage::Index, .DebugName = "TriangleIndices"}, triangleIndices);
		}

		m_QuadShader = ShaderLibrary::Load("Assets/Shaders/TexturedQuad.slang");

		RHI::GraphicsPipelineDesc quadDesc;
		quadDesc.ShaderModule = m_QuadShader;
		quadDesc.ColorFormats[0] = RHI::Format::RGBA8Unorm;
		quadDesc.ColorFormats[1] = RHI::Format::R32I;
		quadDesc.ColorCount = 2;
		triangleDesc.DepthFormat = RHI::Format::D32F;
		triangleDesc.Depth = {.Test = true, .Write = true};
		quadDesc.Raster.Cull = RHI::CullMode::None;
		quadDesc.DebugName = "TexturedQuadPipeline";
		quadDesc.VertexLayout = {{ShaderDataType::Float2, "a_Position"}, {ShaderDataType::Float2, "a_TexCoord"}};
		m_QuadPipeline = RHI::GraphicsPipeline::Create(quadDesc);

		struct QuadVertex
		{
			glm::vec2 Position;
			glm::vec2 UV;
		};

		constexpr QuadVertex quadVertices[]{
			{{-0.5f, -0.5f}, {0.0f, 1.0f}},
			{{0.5f, -0.5f}, {1.0f, 1.0f}},
			{{0.5f, 0.5f}, {1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 0.0f}},
		};
		constexpr uint32_t quadIndices[]{0, 1, 2, 2, 3, 0};

		{
			RHI::UploadScope upload;

			m_QuadVertexBuffer = RHI::GpuBuffer::CreateWithData(
				{.Size = sizeof(quadVertices), .Usage = RHI::BufferUsage::Vertex, .DebugName = "QuadVertices"}, quadVertices);
			m_QuadIndexBuffer = RHI::GpuBuffer::CreateWithData(
				{.Size = sizeof(quadIndices), .Usage = RHI::BufferUsage::Index, .DebugName = "QuadIndices"}, quadIndices);
		}

		// 0xAABBGGRR. Every channel mid-tone on purpose: sRGB decoding only shows on values away from
		// 0 and 1, and every icon in Resources/ is a black glyph carried entirely by its alpha.
		constexpr uint32_t quadSwatch[]{0xFF808080, 0xFF4040C0, 0xFF40C040, 0xFFC04040};

		m_QuadTexture = RHI::Texture::CreateWithData({.Width = 2,
														 .Height = 2,
														 .PixelFormat = RHI::Format::RGBA8Unorm,
														 .MipLevels = 1,
														 .SRGBView = true,
														 .DebugName = "QuadBringUpSwatch"},
			quadSwatch, sizeof(quadSwatch));

		ShaderLibrary::EnableHotReload("Assets/Shaders");

		m_EditorScene = CreateRef<Scene>();

		auto commandLineArgs = Application::Get().GetSpecification().CommandLineArgs;
		if (commandLineArgs.Count > 1)
		{
			std::filesystem::path projectFilePath = commandLineArgs[1];
			if (std::filesystem::exists(projectFilePath))
				OpenProject(projectFilePath);
		}
		else
		{
			// TODO: Prompt the user to select a directory
			// NewProject();

			if (!OpenProject())
				Application::Get().Shutdown();
		}

		m_EditorCamera = EditorCamera(30.0f, 1.778f, 0.1f, 1000.0f);

		m_SceneHierarchyPanel.SetContext(GetActiveScene());
	}

	void EditorLayer::OnDetach()
	{
		TF_PROFILE_FUNCTION();

		ShaderLibrary::Shutdown();

		m_TriangleShader.reset();
		m_TrianglePipeline.reset();
		m_TriangleVertexBuffer.reset();
		m_TriangleIndexBuffer.reset();

		m_QuadShader.reset();
		m_QuadPipeline.reset();
		m_QuadVertexBuffer.reset();
		m_QuadIndexBuffer.reset();
		m_QuadTexture.reset();

		m_ViewportTarget.reset();
		s_Font.reset(); // file-scope, so it would otherwise outlive the device along with its atlas
	}

	Ref<Scene> EditorLayer::GetActiveScene() const
	{
		return m_SceneState == SceneState::Edit ? m_EditorScene : SceneManager::GetActiveScene();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		TF_PROFILE_FUNCTION();

		if (m_TrianglePipeline && m_TrianglePipeline->IsValid())
		{
			if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f
				&& m_ViewportTarget->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y))
			{
				m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			}

			if (RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList(); cmd && m_ViewportTarget->IsValid())
			{
				cmd->BeginPass({.DebugName = "ViewportPass",
					.Target = m_ViewportTarget.get(),
					.Color = {{.Load = RHI::LoadOp::Clear, .ClearValue = {0.1f, 0.1f, 0.12f, 1.0f}},
						{.Load = RHI::LoadOp::Clear, .ClearInt = {-1, -1, -1}}},
					.Depth = {.Load = RHI::LoadOp::Clear, .ClearDepth = 1.0f}});
				
				struct
				{
					glm::vec3 Tint;
					float Time;
				} push{{1.0f, 1.0f, 1.0f}, (float)ImGui::GetTime()};

				cmd->BindPipeline(*m_TrianglePipeline);
				cmd->PushConstants(&push, sizeof(push));

				if (m_TriangleVertexBuffer && m_TriangleVertexBuffer->IsValid())
				{
					cmd->BindVertexBuffer(*m_TriangleVertexBuffer);
					cmd->BindIndexBuffer(*m_TriangleIndexBuffer, RHI::IndexType::U32);
					cmd->DrawIndexed(3);
				}

				struct BringUpVertex
				{
					glm::vec3 Position;
					glm::vec3 Color;
				};

				// Streamed quad: fresh vertices into this frame's slot, proving the ring resets and
				// stays coherent across both slots. Flicker here means the slot arithmetic is wrong.
				const float pulse = 0.5f + 0.5f * std::sin(push.Time * 2.0f);
				const BringUpVertex quadVertices[]{{{0.1f, -0.6f, 0.0f}, {pulse, 0.2f, 1.0f - pulse}},
					{{0.9f, -0.6f, 0.0f}, {1.0f - pulse, pulse, 0.2f}}, {{0.9f, 0.6f, 0.0f}, {0.2f, 1.0f - pulse, pulse}},
					{{0.1f, 0.6f, 0.0f}, {pulse, pulse, pulse}}};
				const uint32_t quadIndices[]{0, 1, 2, 2, 3, 0};

				const RHI::FrameAllocation vertices = RHI::FrameAllocator::Allocate(sizeof(quadVertices));
				const RHI::FrameAllocation indices = RHI::FrameAllocator::Allocate(sizeof(quadIndices), sizeof(uint32_t));

				if (vertices.IsValid() && indices.IsValid())
				{
					std::memcpy(vertices.Mapped, quadVertices, sizeof(quadVertices));
					std::memcpy(indices.Mapped, quadIndices, sizeof(quadIndices));
					cmd->BindVertexBuffer(*vertices.Buffer, vertices.Offset);
					cmd->BindIndexBuffer(*indices.Buffer, RHI::IndexType::U32, indices.Offset);
					cmd->DrawIndexed(6);
				}

				const Ref<Texture2D> atlas = s_Font ? s_Font->GetAtlasTexture() : nullptr;
				const bool quadReady = m_QuadPipeline && m_QuadPipeline->IsValid() && m_QuadVertexBuffer && m_QuadVertexBuffer->IsValid()
					&& m_QuadTexture && m_QuadTexture->IsValid() && atlas && atlas->GetRHITexture();

				if (quadReady)
				{
					struct
					{
						glm::mat4 ViewProjection;
						float Time;
					} frame{};

					const float aspect = m_ViewportSize.y > 0.0f ? m_ViewportSize.x / m_ViewportSize.y : 1.778f;
					frame.ViewProjection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f)
						* glm::rotate(glm::mat4(1.0f), 0.3f * (float)ImGui::GetTime(), glm::vec3(0.0f, 0.0f, 1.0f));
					frame.Time = (float)ImGui::GetTime();

					RHI::Bindings::WriteFrameUniforms(&frame, sizeof(frame));

					cmd->BindPipeline(*m_QuadPipeline);
					cmd->BindVertexBuffer(*m_QuadVertexBuffer);
					cmd->BindIndexBuffer(*m_QuadIndexBuffer, RHI::IndexType::U32);

					struct QuadPush
					{
						glm::vec2 Center;
						float Scale;
						uint32_t TextureIndex;
						uint32_t SamplerSlot;
					};
					static_assert(sizeof(QuadPush) == 20);

					// One image through both its views, then a second texture: the middle quad must come
					// out visibly darker than the left, and the right proves a second bindless slot.
					// Nearest on the 2x2 swatch keeps the quadrants flat and the comparison honest.
					constexpr uint32_t nearestClamp = (uint32_t)RHI::SamplerSlot::NearestClampEdge;
					constexpr uint32_t linearClamp = (uint32_t)RHI::SamplerSlot::LinearClampEdge;

					const QuadPush quads[]{
						{{-1.0f, 0.0f}, 0.5f, m_QuadTexture->GetBindlessIndex(false), nearestClamp},
						{{0.0f, 0.0f}, 0.5f, m_QuadTexture->GetBindlessIndex(true), nearestClamp},
						{{1.0f, 0.0f}, 0.5f, atlas->GetRHITexture()->GetBindlessIndex(false), linearClamp},
					};

					for (const auto& push : quads)
					{
						cmd->PushConstants(&push, sizeof(push));
						cmd->DrawIndexed(6);
					}
				}

				cmd->EndPass();
			}
		}

		GetActiveScene()->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		SceneManager::SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

		// Render
		Renderer3D::SetTargetRenderTarget(m_ViewportTarget);
		Renderer2D::ResetStats();

		// Feed the viewport-relative mouse (top-left origin) to the engine so scripts get world input.
		{
			auto [mouseScreenX, mouseScreenY] = ImGui::GetMousePos();
			Input::SetViewportMousePosition({mouseScreenX - m_ViewportBounds[0].x, mouseScreenY - m_ViewportBounds[0].y});
		}

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_EditorCamera.OnUpdate(ts);
				GetActiveScene()->OnUpdateEditor(ts, m_EditorCamera);
				break;
			}
			case SceneState::Simulate:
			{
				m_EditorCamera.OnUpdate(ts);
				GetActiveScene()->OnUpdateSimulation(ts, m_EditorCamera);
				break;
			}
			case SceneState::Play:
			{
				GetActiveScene()->OnUpdateRuntime(ts);
				if (SceneManager::ProcessPendingLoad())
					m_SceneHierarchyPanel.SetContext(SceneManager::GetActiveScene());
				break;
			}
		}

		m_HoveredEntity = Entity();

		OnOverlayRender();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		TF_PROFILE_FUNCTION();

		m_CameraController.OnEvent(e);

		if (m_SceneState == SceneState::Edit)
			m_EditorCamera.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<KeyPressedEvent>(TF_BIND_EVENT_FN(EditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(TF_BIND_EVENT_FN(EditorLayer::OnMouseButtonPressed));
		dispatcher.Dispatch<WindowDropEvent>(TF_BIND_EVENT_FN(EditorLayer::OnWindowDrop));
	}

	void EditorLayer::OnImGuiRender()
	{
		static bool opt_fullscreen = true;
		static bool opt_padding = false;
		static bool dockspaceOpen = true;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

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
		ImGuiStyle& style = ImGui::GetStyle();
		float minWinSizeX = style.WindowMinSize.x;
		style.WindowMinSize.x = 370.0f;
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		style.WindowMinSize.x = minWinSizeX;

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Project...", "Ctrl + O"))
					OpenProject();

				ImGui::Separator();

				if (ImGui::MenuItem("New Scene", "Ctrl + N"))
					NewScene();

				if (ImGui::MenuItem("Save Scene", "Ctrl + S"))
					SaveScene();

				if (ImGui::MenuItem("Save Scene As...", "Ctrl + Shift + S"))
					SaveSceneAs();

				ImGui::Separator();

				if (ImGui::MenuItem("Exit", NULL, false))
				{
					dockspaceOpen = false;
					Application::Get().Shutdown();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Script"))
			{
				if (ImGui::MenuItem("Reload Assembly", "Ctrl + R"))
					ScriptEngine::ReloadAssembly();

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}

		m_SceneHierarchyPanel.OnImGuiRender();
		m_ContentBrowserPanel->OnImGuiRender(GetActiveScene());
		m_ShadowSettingsPanel.OnImGuiRender(GetActiveScene());
		m_PostProcessSettingsPanel.OnImGuiRender(GetActiveScene());
		m_ProfilerPanel.OnImGuiRender();

		ImGui::Begin("Settings");
		ImGui::Checkbox("Show Physics Colliders", &m_ShowPhysicsColliders);
		ImGui::Image(UI::GetTextureID(s_Font->GetAtlasTexture()), {512, 512}, {0, 1}, {1, 0});
		ImGui::End();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBackground);

		ImVec2 viewportOffset = ImGui::GetCursorScreenPos(); // absolute top-left of the content region
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		m_ViewportBounds[0] = {viewportOffset.x, viewportOffset.y};
		m_ViewportBounds[1] = {viewportOffset.x + viewportPanelSize.x, viewportOffset.y + viewportPanelSize.y};

		m_ViewportFocused = ImGui::IsWindowFocused();
		m_ViewportHovered = ImGui::IsWindowHovered();

		Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered);

		m_ViewportSize = {viewportPanelSize.x, viewportPanelSize.y};

		if (m_ViewportTarget->IsValid())
			ImGui::Image(UI::GetTextureID(m_ViewportTarget->GetColor(0)), viewportPanelSize);

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				AssetHandle handle = *(AssetHandle*)payload->Data;
				OpenScene(handle);
			}
			ImGui::EndDragDropTarget();
		}

		// Gizmos
		if (m_SceneState == SceneState::Edit)
		{
			Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
			if (selectedEntity && m_GizmoType != -1)
			{
				ImGuizmo::SetOrthographic(false);
				ImGuizmo::SetDrawlist();
				ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportBounds[1].x - m_ViewportBounds[0].x,
					m_ViewportBounds[1].y - m_ViewportBounds[0].y);

				// Runtime Camera
				// auto cameraEntity = GetActiveScene()->GetPrimaryCameraEntity();
				// const auto& camera = cameraEntity.GetComponent<CameraComponent>().Camera;
				// const glm::mat4& cameraProjection = camera.GetProjection();
				// glm::mat4 cameraView = glm::inverse(cameraEntity.GetWorldTransform());

				// Editor Camera
				const glm::mat4& cameraProjection = m_EditorCamera.GetProjection();
				glm::mat4 cameraView = m_EditorCamera.GetViewMatrix();

				// Entity transform — operate in WORLD space so the gizmo tracks the entity's actual
				// on-screen location even when it's a child (local != world).
				glm::mat4 transform = selectedEntity.GetWorldTransform();

				// Snapping
				bool snap = Input::IsKeyPressed(Key::LeftControl);
				float snapValue = 0.5f; // Snap to 0.5m for translation/scale
				// Snap to 5 degrees for rotation
				if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
					snapValue = 5.0f;

				float snapValues[3] = {snapValue, snapValue, snapValue};

				ImGuizmo::Manipulate(glm::value_ptr(cameraView), glm::value_ptr(cameraProjection), (ImGuizmo::OPERATION)m_GizmoType,
					ImGuizmo::LOCAL, glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

				if (ImGuizmo::IsUsing())
				{
					// Write the manipulated world transform back; SetWorldTransform solves the
					// entity's local transform under its parent (identity for roots).
					selectedEntity.SetWorldTransform(transform);
				}
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();

		UI_Toolbar();

		ImGui::End();
	}

	void EditorLayer::UI_Toolbar()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 2});
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{0, 0});
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{0, 0});

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
		auto& color = ImGui::GetStyle().Colors;
		const auto& buttonHovered = color[ImGuiCol_ButtonHovered];
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{buttonHovered.x, buttonHovered.y, buttonHovered.z, 0.5f});
		const auto& buttonActive = color[ImGuiCol_ButtonActive];
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{buttonActive.x, buttonActive.y, buttonActive.z, 0.5f});

		ImGui::Begin(
			"##Toolbar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

		bool toolbarEnabled = (bool)GetActiveScene();

		ImVec4 tintColor = ImVec4(1, 1, 1, 1);
		if (!toolbarEnabled)
			tintColor.w = 0.5f;

		float size = ImGui::GetWindowHeight() - 4.0f;
		ImGui::SetCursorPosX(((ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x) * 0.5f) - (size * 0.5f));

		bool hasPlayButton = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play;
		bool hasSimulateButton = m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate;
		bool hasPauseButton = m_SceneState != SceneState::Edit;

		// Play
		if (hasPlayButton)
		{
			Ref<Texture2D> icon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate) ? m_PlayIcon : m_StopIcon;
			if (ImGui::ImageButton("##toolbar_play", UI::GetTextureID(icon), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor)
				&& toolbarEnabled)
			{
				if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Simulate)
					OnScenePlay();
				else if (m_SceneState == SceneState::Play)
					OnSceneStop();
			}
		}

		// Simulate
		if (hasSimulateButton)
		{
			if (hasPlayButton)
				ImGui::SameLine();

			Ref<Texture2D> icon = (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play) ? m_SimulateIcon : m_StopIcon;
			if (ImGui::ImageButton("##toolbar_simulate", UI::GetTextureID(icon), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
					ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor)
				&& toolbarEnabled)
			{
				if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Play)
					OnSceneSimulate();
				else if (m_SceneState == SceneState::Simulate)
					OnSceneStop();
			}
		}

		if (hasPauseButton)
		{
			bool isPaused = GetActiveScene()->IsPaused();
			ImGui::SameLine();
			{
				Ref<Texture2D> icon = m_PauseIcon;
				if (ImGui::ImageButton("##toolbar_pause", UI::GetTextureID(icon), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
						ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor)
					&& toolbarEnabled)
					GetActiveScene()->SetPaused(!isPaused);
			}

			// Step button
			if (isPaused)
			{
				ImGui::SameLine();
				{
					Ref<Texture2D> icon = m_StepIcon;
					if (ImGui::ImageButton("##toolbar_step", UI::GetTextureID(icon), ImVec2(size, size), ImVec2(0, 0), ImVec2(1, 1),
							ImVec4(0.0f, 0.0f, 0.0f, 0.0f), tintColor)
						&& toolbarEnabled)
						GetActiveScene()->Step();
				}
			}
		}

		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor(3);

		ImGui::End();
	}

	bool EditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		if (e.GetRepeatCount() > 0)
			return false;

		bool control = Input::IsKeyPressed(KeyCode::LeftControl) || Input::IsKeyPressed(KeyCode::RightControl);
		bool shift = Input::IsKeyPressed(KeyCode::LeftShift) || Input::IsKeyPressed(KeyCode::RightShift);

		switch (e.GetKeyCode())
		{
			case KeyCode::S:
			{
				if (control)
				{
					if (shift)
						SaveSceneAs();
					else
						SaveScene();
				}

				break;
			}
			case KeyCode::D:
			{
				if (control)
					DuplicateEntity();

				break;
			}
			case KeyCode::N:
			{
				if (control)
					NewScene();

				break;
			}
			case KeyCode::O:
			{
				if (control)
					OpenProject();

				break;
			}

			// Gizmos
			case Key::Q:
				if (!ImGuizmo::IsUsing())
					m_GizmoType = -1;
				break;
			case Key::W:
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;
			case Key::E:
				if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::ROTATE;
				break;
			case Key::R:
				if (control)
					ScriptEngine::ReloadAssembly();
				else if (!ImGuizmo::IsUsing())
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
				break;
			case Key::Delete:
			{
				if (Application::Get().GetImGuiLayer()->GetActiveWidgetID() == 0)
				{
					Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
					if (selectedEntity)
					{
						m_SceneHierarchyPanel.SetSelectedEntity({});
						GetActiveScene()->DestroyEntity(selectedEntity);
					}
				}
				break;
			}
		}

		return false;
	}

	bool EditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		if (e.GetMouseButton() == MouseCode::LeftButton)
		{
			if (m_ViewportHovered && !ImGuizmo::IsOver() && !Input::IsKeyPressed(KeyCode::LeftAlt))
				m_SceneHierarchyPanel.SetSelectedEntity(m_HoveredEntity);
		}

		return false;
	}

	bool EditorLayer::OnWindowDrop(WindowDropEvent& e)
	{
		// TODO: if a project is dropped in, probably open it

		// AssetManager::ImportAsset();

		return true;
	}

	namespace
	{
		constexpr float GIZMO_TWO_PI = 6.28318530718f;

		// Orthonormal basis (right, up) spanning the plane perpendicular to `axis`.
		static void MakeBasis(const glm::vec3& axis, glm::vec3& right, glm::vec3& up)
		{
			glm::vec3 a = glm::normalize(axis);
			glm::vec3 ref = (glm::abs(a.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
			right = glm::normalize(glm::cross(ref, a));
			up = glm::cross(a, right);
		}

		// A wireframe circle of `radius` centered at `center`, lying in the plane perpendicular to `axis`.
		static void DrawWireCircle(const glm::vec3& center, const glm::vec3& axis, float radius, const glm::vec4& color, int segments = 24)
		{
			glm::vec3 right, up;
			MakeBasis(axis, right, up);

			glm::vec3 prev(0.0f);
			for (int i = 0; i <= segments; i++)
			{
				float t = (float)i / (float)segments * GIZMO_TWO_PI;
				glm::vec3 p = center + radius * (std::cos(t) * right + std::sin(t) * up);
				if (i > 0)
					Renderer2D::DrawLine(prev, p, color);
				prev = p;
			}
		}

		// Sun gizmo: a ring with parallel rays along the light direction.
		static void DrawDirectionalLightGizmo(const glm::vec3& pos, const glm::vec3& dir, const glm::vec4& color)
		{
			const float ringRadius = 0.25f;
			const float rayLength = 1.0f;

			DrawWireCircle(pos, dir, ringRadius, color);

			glm::vec3 right, up;
			MakeBasis(dir, right, up);
			for (int i = 0; i < 4; i++)
			{
				float t = (float)i / 4.0f * GIZMO_TWO_PI;
				glm::vec3 base = pos + ringRadius * (std::cos(t) * right + std::sin(t) * up);
				Renderer2D::DrawLine(base, base + dir * rayLength, color);
			}
			Renderer2D::DrawLine(pos, pos + dir * rayLength, color);
		}

		// Point gizmo: three rings forming a wireframe sphere.
		static void DrawPointLightGizmo(const glm::vec3& pos, const glm::vec4& color)
		{
			const float r = 0.3f;
			DrawWireCircle(pos, glm::vec3(1.0f, 0.0f, 0.0f), r, color);
			DrawWireCircle(pos, glm::vec3(0.0f, 1.0f, 0.0f), r, color);
			DrawWireCircle(pos, glm::vec3(0.0f, 0.0f, 1.0f), r, color);
		}

		// Spot gizmo: a cone opening along the direction, half-angle = outer cutoff, length = range.
		static void DrawSpotLightGizmo(
			const glm::vec3& pos, const glm::vec3& dir, float outerCutoffDeg, float range, const glm::vec4& color)
		{
			float length = range;
			float radius = length * std::tan(outerCutoffDeg * (GIZMO_TWO_PI / 360.0f)); // deg -> rad
			glm::vec3 endCenter = pos + dir * length;

			DrawWireCircle(endCenter, dir, radius, color);

			glm::vec3 right, up;
			MakeBasis(dir, right, up);
			for (int i = 0; i < 4; i++)
			{
				float t = (float)i / 4.0f * GIZMO_TWO_PI;
				glm::vec3 edge = endCenter + radius * (std::cos(t) * right + std::sin(t) * up);
				Renderer2D::DrawLine(pos, edge, color);
			}
		}
	}

	void EditorLayer::OnOverlayRender()
	{
		if (m_SceneState == SceneState::Play)
		{
			Entity camera = GetActiveScene()->GetPrimaryCameraEntity();
			if (!camera)
				return;

			Renderer2D::BeginScene(
				camera.GetComponent<CameraComponent>().Camera, camera.GetComponent<TransformComponent>().GetLocalTransform());
		}
		else
		{
			Renderer2D::BeginScene(m_EditorCamera);
		}

		if (m_ShowPhysicsColliders)
		{
			// Box Colliders
			{
				auto view = GetActiveScene()->GetAllEntitiesWithUsingView<TransformComponent, BoxCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, bc2d] = view.get<TransformComponent, BoxCollider2DComponent>(entity);

					glm::vec3 scale = tc.Scale * glm::vec3(bc2d.Size * 2.0f, 1.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.Translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::translate(glm::mat4(1.0f), glm::vec3(bc2d.Offset, 0.001f)) // Center the box collider
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawRect(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
			}

			// Circle Colliders
			{
				float zIndex = 0.001f; // Slightly above the circle colliders to avoid z-fighting
				glm::vec3 cameraForwardDirection = m_EditorCamera.GetForwardDirection();
				glm::vec3 projectionCollider = cameraForwardDirection * zIndex;

				auto view = GetActiveScene()->GetAllEntitiesWithUsingView<TransformComponent, CircleCollider2DComponent>();
				for (auto entity : view)
				{
					auto [tc, cc2d] = view.get<TransformComponent, CircleCollider2DComponent>(entity);

					glm::vec3 scale = tc.Scale * glm::vec3(cc2d.Radius * 2.0f);

					glm::mat4 transform = glm::translate(glm::mat4(1.0f), tc.Translation)
						* glm::rotate(glm::mat4(1.0f), tc.Rotation.z, glm::vec3(0.0f, 0.0f, 1.0f))
						* glm::translate(glm::mat4(1.0f), glm::vec3(cc2d.Offset, -projectionCollider.z)) // Center the circle collider
						* glm::scale(glm::mat4(1.0f), scale);

					Renderer2D::DrawCircle(transform, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f), 0.01f);
				}
			}
		}

		// Light gizmo — shown only for the selected light (editor-only; lights have no geometry of
		// their own). Drawn in the light's color so it doubles as a color readout.
		if (m_SceneState != SceneState::Play)
		{
			if (Entity selected = m_SceneHierarchyPanel.GetSelectedEntity(); selected.IsValid() && selected.HasComponent<LightComponent>())
			{
				auto& lc = selected.GetComponent<LightComponent>();

				glm::mat4 world = selected.GetWorldTransform();
				glm::vec3 pos = glm::vec3(world[3]);
				glm::vec3 dir = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
				glm::vec4 color(lc.Color, 1.0f);

				switch (lc.Type)
				{
					case LightComponent::LightType::Directional: DrawDirectionalLightGizmo(pos, dir, color); break;
					case LightComponent::LightType::Point: DrawPointLightGizmo(pos, color); break;
					case LightComponent::LightType::Spot: DrawSpotLightGizmo(pos, dir, lc.OuterCutoff, lc.Range, color); break;
				}
			}
		}

		// Draw selected entity outline — an editor-only aid, so skip it in Play mode where the
		// runtime camera shows the actual game (otherwise the orange rect leaks onto the screen).
		// Edit and Simulate both use the editor camera, so the outline still draws there.
		// IsValid() guards against a selection that was destroyed this frame (e.g. by a script via
		// the deferred-destroy queue) but is still cached here.
		if (m_SceneState != SceneState::Play)
		{
			if (Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity(); selectedEntity.IsValid())
			{
				// World transform so the outline sits on the entity's actual location, child or not.
				glm::mat4 worldTransform = selectedEntity.GetWorldTransform();
				const glm::vec4 outlineColor(1.0f, 0.5f, 0.0f, 1.0f);

				if (selectedEntity.HasComponent<MeshComponent>())
				{
					// Wireframe box around the submesh's actual local-space bounds, resolved from the
					// mesh asset, so imported meshes of any size/offset get a correctly-fitted outline.
					// Falls back to a unit box if the mesh can't be resolved.
					glm::vec3 min(-0.5f), max(0.5f);
					const auto& mc = selectedEntity.GetComponent<MeshComponent>();
					if (mc.Mesh != 0 && AssetManager::IsAssetHandleValid(mc.Mesh))
					{
						Ref<MeshSource> meshSource = AssetManager::GetAsset<MeshSource>(mc.Mesh);
						if (meshSource && mc.Submesh < meshSource->GetSubmeshes().size())
						{
							const Submesh& sm = meshSource->GetSubmeshes()[mc.Submesh];
							min = sm.MinBounds;
							max = sm.MaxBounds;
						}
					}

					glm::vec3 corners[8] = {
						{min.x, min.y, min.z},
						{max.x, min.y, min.z},
						{max.x, max.y, min.z},
						{min.x, max.y, min.z},
						{min.x, min.y, max.z},
						{max.x, min.y, max.z},
						{max.x, max.y, max.z},
						{min.x, max.y, max.z},
					};
					for (int i = 0; i < 8; i++)
						corners[i] = glm::vec3(worldTransform * glm::vec4(corners[i], 1.0f));

					static const int edges[12][2] = {
						{0, 1},
						{1, 2},
						{2, 3},
						{3, 0}, // z = min face
						{4, 5},
						{5, 6},
						{6, 7},
						{7, 4}, // z = max face
						{0, 4},
						{1, 5},
						{2, 6},
						{3, 7}, // connectors
					};
					for (const auto& e : edges)
						Renderer2D::DrawLine(corners[e[0]], corners[e[1]], outlineColor);
				}
				else if (selectedEntity.HasComponent<LightComponent>())
				{
					// Lights are highlighted by their gizmo (drawn in the selection color above);
					// the flat 2D rect would sit at the center and be misleading, so skip it.
				}
				else
				{
					Renderer2D::DrawRect(worldTransform, outlineColor);
				}
			}
		}

		Renderer2D::EndScene();
	}

	void EditorLayer::NewProject()
	{
		Project::New();
	}

	bool EditorLayer::OpenProject()
	{
		auto filepath = FileDialogs::OpenFile("Timefall Project (*.tfproj)\0*.tfproj\0");
		if (!filepath.empty())
			OpenProject(filepath);

		return !filepath.empty();
	}

	void EditorLayer::OpenProject(const std::filesystem::path& filepath)
	{
		if (Project::Load(filepath))
		{
			ScriptEngine::Init();

			AssetHandle startScene = Project::GetActive()->GetConfig().StartScene;
			if (startScene)
				OpenScene(startScene);

			// Seed the built-in primitive meshes BEFORE the Content Browser is created, because
			// its constructor snapshots the asset registry into its tree (RefreshAssetTree).
			Renderer3D::RegisterBuiltInMeshes(*Project::GetActive()->GetEditorAssetManager());
			m_ContentBrowserPanel = CreateScope<ContentBrowserPanel>();
		}
	}

	void EditorLayer::SaveProject()
	{
		// Project::SaveActive();
	}

	void EditorLayer::NewScene()
	{
		m_EditorScene = CreateRef<Scene>();

		m_SceneHierarchyPanel.SetContext(m_EditorScene);

		m_EditorScenePath = std::filesystem::path();
	}

	void EditorLayer::OpenScene()
	{
		// auto filepath = FileDialogs::OpenFile("Timefall Scene (*.timefall)\0*.timefall\0");
		// if (!filepath.empty())
		// 	OpenScene(filepath);
	}

	void EditorLayer::OpenScene(AssetHandle handle)
	{
		TF_CORE_ASSERT(handle != (AssetHandle)0, "Invalid asset handle!")

		if (m_SceneState != SceneState::Edit)
			OnSceneStop();

		Ref<Scene> readOnlyScene = AssetManager::GetAsset<Scene>(handle);
		Ref<Scene> newScene = Scene::Copy(readOnlyScene);

		m_EditorScene = newScene;
		m_SceneHierarchyPanel.SetContext(m_EditorScene);

		m_EditorScenePath = Project::GetActive()->GetEditorAssetManager()->GetFilePath(handle);
	}

	void EditorLayer::SaveScene()
	{
		if (!m_EditorScenePath.empty())
			SerializeScene(m_EditorScene, m_EditorScenePath);
		else
			SaveSceneAs();
	}

	void EditorLayer::SaveSceneAs()
	{
		auto filepath = FileDialogs::SaveFile("Timefall Scene (*.timefall)\0*.timefall\0");
		if (!filepath.empty())
		{
			SerializeScene(m_EditorScene, filepath);
			m_EditorScenePath = filepath;
		}
	}

	void EditorLayer::DuplicateEntity()
	{
		if (m_SceneState != SceneState::Edit)
			return;

		Entity selectedEntity = m_SceneHierarchyPanel.GetSelectedEntity();
		if (selectedEntity)
		{
			Entity newEntity = m_EditorScene->DuplicateEntity(selectedEntity);
			m_SceneHierarchyPanel.SetSelectedEntity(newEntity);
		}
	}

	void EditorLayer::SerializeScene(const Ref<Scene>& scene, const std::filesystem::path& filepath)
	{
		SceneImporter::SaveScene(scene, filepath);
	}

	void EditorLayer::OnScenePlay()
	{
		if (m_SceneState == SceneState::Simulate)
			OnSceneStop();

		m_SceneState = SceneState::Play;

		Ref<Scene> runtime = Scene::Copy(m_EditorScene);
		SceneManager::SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		SceneManager::SetActiveScene(runtime);
		runtime->OnRuntimeStart();

		m_SceneHierarchyPanel.SetContext(runtime);
	}

	void EditorLayer::OnSceneSimulate()
	{
		if (m_SceneState == SceneState::Play)
			OnSceneStop();

		m_SceneState = SceneState::Simulate;

		Ref<Scene> runtime = Scene::Copy(m_EditorScene);
		SceneManager::SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		SceneManager::SetActiveScene(runtime);
		runtime->OnSimulationStart();

		m_SceneHierarchyPanel.SetContext(runtime);
	}

	void EditorLayer::OnSceneStop()
	{
		TF_CORE_ASSERT(m_SceneState == SceneState::Play || m_SceneState == SceneState::Simulate, "Invalid scene state!");

		Ref<Scene> runtime = SceneManager::GetActiveScene();
		if (runtime)
		{
			if (m_SceneState == SceneState::Play)
				runtime->OnRuntimeStop();
			else if (m_SceneState == SceneState::Simulate)
				runtime->OnSimulationStop();
		}

		SceneManager::SetActiveScene(nullptr);
		m_SceneState = SceneState::Edit;

		m_SceneHierarchyPanel.SetContext(m_EditorScene);
	}

	void EditorLayer::OnScenePause()
	{
		if (m_SceneState == SceneState::Edit)
			return;

		GetActiveScene()->SetPaused(true);
	}
}
