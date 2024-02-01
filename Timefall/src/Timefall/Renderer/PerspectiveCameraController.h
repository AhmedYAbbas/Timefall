#pragma once

#include "Timefall/Renderer/Camera.h"
#include "Timefall/Core/Timestep.h"

#include "Timefall/Events/ApplicationEvent.h"
#include "Timefall/Events/MouseEvent.h"

namespace Timefall
{
	class PerspectiveCameraController
	{
	public:
		PerspectiveCameraController(float fov, float aspectRatio);

		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		bool OnMouseMoved(MouseMovedEvent& e);
		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);

		PerspectiveCamera& GetCamera() { return m_Camera; }
		const PerspectiveCamera& GetCamera() const { return m_Camera; }
	private:
		float m_FOV;
		float m_AspectRatio;
		PerspectiveCamera m_Camera;

		glm::vec3 m_CameraPosition = {0.f, 0.f, 3.f};
		float m_Yaw = -90.f, m_Pitch = 0.f, m_Roll = 0.f;
		float m_CameraSpeed = 5.f;

		float m_LastX = 0.f, m_LastY = 0.f;
		float m_Sensitivity = 0.1f;
		bool m_FirstMouse = true;

		glm::vec3 m_CameraFront = glm::vec3(0.f, 0.f, -1.f);
		glm::vec3 m_CameraUp = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 m_CameraRight = glm::vec3(1.f, 0.f, 0.f);
	};
}