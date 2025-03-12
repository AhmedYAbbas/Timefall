#pragma once

#include "Timefall/Renderer/Camera.h"
#include "Timefall/Core/Timestep.h"

#include "Timefall/Events/ApplicationEvent.h"
#include "Timefall/Events/MouseEvent.h"

namespace Timefall
{
	class OrthographicCameraController
	{
	public:
		OrthographicCameraController(float aspectRatio, bool rotation = false);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		void OnResize(float width, float height);

		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);

		void SetZoomLevel(float zoomLevel);
		const float GetZoomLevel() const { return m_ZoomLevel; }

		void CalculateView();

		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const { return m_Camera; }
	private:
		float m_AspectRatio;
		float m_ZoomLevel = 1;
		OrthographicCamera m_Camera;						

		bool m_Rotation;

		float m_CameraRotation = 0.0f; //In degrees, in the anti-clockwise direction
		glm::vec3 m_CameraPosition = {0.0f, 0.0f, 0.0f};
		float m_CameraTranslationSpeed = 5.0f, m_CameraRotationSpeed = 90.0f;
	};
}