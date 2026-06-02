#pragma once

#include <glm/glm.hpp>

namespace Timefall::Math
{
	TF_API bool DecomposeTransform(const glm::mat4& transform, glm::vec3& position, glm::vec3& rotation, glm::vec3& scale);

	// Composes a TRS matrix (translate * rotate * scale). Rotation is Euler radians.
	// Inverse of DecomposeTransform; the single home for local-transform composition now that
	// TransformComponent is a pure data container.
	TF_API glm::mat4 ComposeTransform(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale);
}
