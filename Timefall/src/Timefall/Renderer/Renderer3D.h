#pragma once

#include "Timefall/Renderer/EditorCamera.h"
#include "Timefall/Renderer/Camera.h"
#include "Timefall/Renderer/Mesh.h"

#include <glm/glm.hpp>

namespace Timefall
{
	// Phase 9.1: ECS-driven immediate-mode forward renderer over built-in primitives.
	class TF_API Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		// Editor viewport camera.
		static void BeginScene(const EditorCamera& camera);
		// Runtime camera: scene camera projection + the camera entity's world transform.
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void EndScene();

		static void SubmitMesh(const glm::mat4& transform, const Ref<Mesh>& mesh, int entityID = -1);

		// Lights are submitted after BeginScene and before any SubmitMesh; they are uploaded
		// to the Lights UBO lazily on the first SubmitMesh of the frame.
		static void SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity);
		static void SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range);
		static void SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
			float intensity, float range, float innerCutoffDegrees, float outerCutoffDegrees);

		// Returns the cached built-in mesh for a primitive type.
		static Ref<Mesh> GetPrimitive(PrimitiveType type);
	};
}
