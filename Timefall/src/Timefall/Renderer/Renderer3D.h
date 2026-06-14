#pragma once

#include "Timefall/Renderer/EditorCamera.h"
#include "Timefall/Renderer/Camera.h"
#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Renderer/Material.h"
#include "Timefall/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Timefall
{
	class EditorAssetManager;

	// Reserved handles for the built-in primitive meshes. Typed as uint64_t (not AssetHandle)
	// because UUID isn't a literal type, so it can't be constexpr; the implicit uint64_t<->UUID
	// conversions make these usable anywhere an AssetHandle is expected.
	namespace BuiltInMesh
	{
		static constexpr uint64_t Cube   = 1;
		static constexpr uint64_t Sphere = 2;
		static constexpr uint64_t Plane  = 3;
	}

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

		static void SubmitMesh(const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex,
			const Ref<Material>& material, int entityID = -1);

		// Fallback material for meshes whose Material handle is 0 / invalid.
		static Ref<Material> GetDefaultMaterial();

		// Seed the three built-in primitive meshes into a project's asset manager as memory-only Mesh assets.
		static void RegisterBuiltInMeshes(EditorAssetManager& assetManager);

		// Lights are submitted after BeginScene and before any SubmitMesh; they are uploaded
		// to the Lights UBO lazily on the first SubmitMesh of the frame.
		static void SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity);
		static void SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range);
		static void SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
			float intensity, float range, float innerCutoffDegrees, float outerCutoffDegrees);
	};
}
