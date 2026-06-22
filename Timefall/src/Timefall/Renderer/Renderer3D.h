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

	namespace BuiltInMesh
	{
		static constexpr uint64_t Cube   = 1;
		static constexpr uint64_t Sphere = 2;
		static constexpr uint64_t Plane  = 3;
	}

	class TF_API Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void EndScene();

		static void SubmitMesh(const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex,
			const Ref<Material>& material, int entityID = -1);

		static Ref<Material> GetDefaultMaterial();

		static void RegisterBuiltInMeshes(EditorAssetManager& assetManager);

		static void SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity,
			bool castsShadows = false, float shadowSoftness = 0.5f, float depthBias = 1.0f);
		static void SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range);
		static void SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color,
			float intensity, float range, float innerCutoffDegrees, float outerCutoffDegrees);
	};
}
