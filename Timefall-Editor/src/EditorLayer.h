#pragma once

#include "Timefall.h"
#include "Panels/SceneHierarchyPanel.h"
#include "Panels/ContentBrowserPanel.h"
#include <filesystem>

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
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnWindowDrop(WindowDropEvent& e);

		void OnOverlayRender();

		void NewProject();
		bool OpenProject();
		void OpenProject(const std::filesystem::path& filepath);
		void SaveProject();

		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& filepath);
		void SaveScene();
		void SaveSceneAs();
		void DuplicateEntity();

		void SerializeScene(const Ref<Scene>& scene, const std::filesystem::path& filepath);

		void UI_Toolbar();

		void OnScenePlay();
		void OnSceneSimulate();
		void OnSceneStop();
		void OnScenePause();

	private:
		OrthographicCameraController m_CameraController;

		// Temp
		Ref<Framebuffer> m_Framebuffer;

		Ref<Scene> m_ActiveScene;
		Ref<Scene> m_EditorScene;
		std::filesystem::path m_EditorScenePath;

		Entity m_SquareEntity;
		Entity m_PrimaryCamera;
		Entity m_SecondaryCamera;

		EditorCamera m_EditorCamera;
		Entity m_HoveredEntity;

		bool m_IsPrimaryCamera = true;

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];

		bool m_ViewportFocused = false, m_ViewportHovered = false;

		int m_GizmoType = -1;

		bool m_ShowPhysicsColliders = false;

		Ref<Texture2D> m_PlayIcon, m_PauseIcon, m_StepIcon, m_SimulateIcon, m_StopIcon;

		enum class SceneState
		{
			Edit = 0, Play = 1, Simulate = 2
		};
		SceneState m_SceneState = SceneState::Edit;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		Scope<ContentBrowserPanel> m_ContentBrowserPanel;
	};
}