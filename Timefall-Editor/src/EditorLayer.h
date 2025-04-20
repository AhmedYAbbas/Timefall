#pragma once

#include "Timefall.h"
#include "Panels/SceneHierarchyPanel.h"

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

		void NewScene();
		void OpenScene();
		void SaveSceneAs();

	private:
		OrthographicCameraController m_CameraController;

		// Temp
		Ref<Framebuffer> m_Framebuffer;

		Ref<Scene> m_ActiveScene;
		Entity m_SquareEntity;
		Entity m_PrimaryCamera;
		Entity m_SecondaryCamera;

		bool m_IsPrimaryCamera = true;

		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		bool m_ViewportFocused = false, m_ViewportHovered = false;

		SceneHierarchyPanel m_SceneHierarchyPanel;
		int m_GizmoType = -1;
	};
}