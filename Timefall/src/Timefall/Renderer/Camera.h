#pragma once

#include <glm/glm.hpp>

namespace Timefall
{
	class TF_API Camera
	{
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			: m_Projection(projection)
		{}

		virtual ~Camera() = default;

		glm::mat4& GetProjection() { return m_Projection; }
		const glm::mat4& GetProjection() const { return m_Projection; }

	protected:
		glm::mat4 m_Projection;
	};
}
