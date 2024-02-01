#include "tfpch.h"
#include "Timefall/Renderer/PerspectiveCameraController.h"

#include "Timefall/Core/Input.h"
#include "Timefall/Core/Keycodes.h"

namespace Timefall
{
	Timefall::PerspectiveCameraController::PerspectiveCameraController(float fov, float aspectRatio)
		: m_FOV(fov), m_AspectRatio(aspectRatio), m_Camera(m_FOV, m_AspectRatio)
	{
	}

	void Timefall::PerspectiveCameraController::OnUpdate(Timestep ts)
	{
		if (Input::IsKeyPressed(TF_KEY_W))
			m_CameraPosition +=  m_CameraSpeed * ts * m_CameraFront;
		if (Input::IsKeyPressed(TF_KEY_S))
			m_CameraPosition -= m_CameraSpeed * ts * m_CameraFront;

		if (Input::IsKeyPressed(TF_KEY_A))
		{
			// Caching the right vector for the camera just in case if I ever need it again
			m_CameraRight = glm::normalize(glm::cross(m_CameraFront, m_CameraUp));
			m_CameraPosition -= m_CameraSpeed * ts * m_CameraRight;
		}
		if (Input::IsKeyPressed(TF_KEY_D))
		{
			// Caching the right vector for the camera just in case if I ever need it again
			m_CameraRight = glm::normalize(glm::cross(m_CameraFront, m_CameraUp));
			m_CameraPosition += m_CameraSpeed * ts * m_CameraRight;
		}

		if (Input::IsKeyPressed(TF_KEY_E))
			m_CameraPosition += m_CameraSpeed * ts * m_CameraUp;
		if (Input::IsKeyPressed(TF_KEY_Q))
			m_CameraPosition -= m_CameraSpeed * ts * m_CameraUp;

		m_Camera.SetView(m_CameraPosition, m_CameraFront, m_CameraUp);
	}

	void Timefall::PerspectiveCameraController::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<MouseScrolledEvent>(TF_BIND_EVENT_FN(&PerspectiveCameraController::OnMouseScrolled));
		dispatcher.Dispatch<WindowResizeEvent>(TF_BIND_EVENT_FN(&PerspectiveCameraController::OnWindowResized));
		dispatcher.Dispatch<MouseMovedEvent>(TF_BIND_EVENT_FN(&PerspectiveCameraController::OnMouseMoved));
	}

	bool PerspectiveCameraController::OnMouseMoved(MouseMovedEvent& e)
	{
		float xPos = e.GetX();
		float yPos = e.GetY();

		if (m_FirstMouse)
		{
			m_LastX = xPos;
			m_LastY = yPos;
			m_FirstMouse = false;
		}

		float xOffset = xPos - m_LastX;
		float yOffset = m_LastY - yPos;
		m_LastX = xPos;
		m_LastY = yPos;

		xOffset *= m_Sensitivity;
		yOffset *= m_Sensitivity;

		m_Yaw += xOffset;
		m_Pitch += yOffset;

		if (m_Pitch > 89.0f)
			m_Pitch = 89.0f;
		if (m_Pitch < -89.0f)
			m_Pitch = -89.0f;

		glm::vec3 front;
		front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		front.y = sin(glm::radians(m_Pitch));
		front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
		m_CameraFront = glm::normalize(front);

		m_Camera.SetView(m_CameraPosition, m_CameraFront, m_CameraUp);

		return false;
	}

	bool PerspectiveCameraController::OnMouseScrolled(MouseScrolledEvent& e)
	{
		m_CameraSpeed += e.GetYOffset() * 0.2f;

		if (m_CameraSpeed <= 1.f)
			m_CameraSpeed = 1.f;
		if (m_CameraSpeed >= 10.f)
			m_CameraSpeed = 10.f;

		return false;
	}

	bool PerspectiveCameraController::OnWindowResized(WindowResizeEvent& e)
	{
		m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
		m_Camera.SetProjection(m_FOV, m_AspectRatio);
		return false;
	}

}