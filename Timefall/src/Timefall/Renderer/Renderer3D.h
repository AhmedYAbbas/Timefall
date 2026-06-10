#pragma once

#include "Timefall/Renderer/EditorCamera.h"

#include <glm/glm.hpp>

namespace Timefall
{
	// Phase 9.0: immediate-mode forward renderer proving the pipeline with a single cube.
	class TF_API Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const EditorCamera& camera);
		static void EndScene();

		// Draws the built-in unit cube (centered at origin) at the given world transform.
		static void DrawTestCube(const glm::mat4& transform = glm::mat4(1.0f));
	};
}
