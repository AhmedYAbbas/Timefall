#pragma once

#include <glm/glm.hpp>

namespace Timefall
{
	class OrthographicCamera
	{
	public:
		OrthographicCamera(float left, float right, float bottom, float top, float zNear = -1, float zFar = 1);

		void SetProjection(float left, float right, float bottom, float top, float zNear = -1, float zFar = 1);

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }
		const glm::vec3& GetPosition() const { return m_Position; }
		float GetRotation() const { return m_Rotation; }

		void SetPosition(const glm::vec3& position);
		void SetRotation(float rotation);

		void RecalculateViewMatrix();

	private:
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position = glm::vec3(0.f, 0.f, 0.f);
		float m_Rotation = 0.f;
	};

	class PerspectiveCamera
	{
	public:
		PerspectiveCamera(float fov, float aspect, float zNear = 0.1f, float zFar = 100.f);

		void SetProjection(float fov, float aspect, float zNear = 0.1f, float zFar = 100.f);
		void SetView(const glm::vec3& position, const glm::vec3& front, const glm::vec3& up);

		const glm::mat4& GetProjectionMatrix() const { return m_ProjectionMatrix; }
		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjectionMatrix; }


	private:
		glm::mat4 m_ProjectionMatrix;
		glm::mat4 m_ViewMatrix;
		glm::mat4 m_ViewProjectionMatrix;

		glm::vec3 m_Position = glm::vec3(0.f, 0.f, 3.f);
		glm::vec3 m_Front = glm::vec3(0.f, 0.f, -1.f);
		glm::vec3 m_Up = glm::vec3(0.f, 1.f, 0.f);
		glm::vec3 m_Right = glm::vec3(1.f, 0.f, 0.f);
	};
}