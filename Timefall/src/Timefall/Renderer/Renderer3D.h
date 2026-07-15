#pragma once

#include "Timefall/Renderer/EditorCamera.h"
#include "Timefall/Renderer/Camera.h"
#include "Timefall/Renderer/Framebuffer.h"
#include "Timefall/Renderer/Mesh.h"
#include "Timefall/Renderer/Material.h"
#include "Timefall/Renderer/ShadowSettings.h"
#include "Timefall/Renderer/PostProcessSettings.h"
#include "Timefall/Asset/Asset.h"

#include <glm/glm.hpp>

namespace Timefall
{
	class EditorAssetManager;

	namespace BuiltInMesh
	{
		static constexpr uint64_t Cube = 1;
		static constexpr uint64_t Sphere = 2;
		static constexpr uint64_t Plane = 3;
	}

	class TF_API Renderer3D
	{
	public:
		static void Init();
		static void Shutdown();

		static void SetTargetFramebuffer(const Ref<Framebuffer>& target);

		static void BeginScene(const EditorCamera& camera);
		static void BeginScene(const Camera& camera, const glm::mat4& transform);
		static void EndScene();

		// Call before EndScene. Recreates the shadow map on resolution / cascade-count change.
		static void SetShadowSettings(const ShadowSettings& settings);
		static void SetPostProcessSettings(const PostProcessSettings& settings);

		static void SubmitMesh(const glm::mat4& transform, const Ref<MeshSource>& mesh, uint32_t submeshIndex,
			const Ref<Material>& material, int entityID = -1);

		static Ref<Material> GetDefaultMaterial();

		static void RegisterBuiltInMeshes(EditorAssetManager& assetManager);

		static void SubmitDirectionalLight(const glm::vec3& direction, const glm::vec3& color, float intensity, bool castsShadows = false,
			float shadowSoftness = 0.5f, float depthBias = 1.0f);
		static void SubmitPointLight(const glm::vec3& position, const glm::vec3& color, float intensity, float range,
			bool castsShadows = false, float shadowSoftness = 0.5f, float depthBias = 1.0f);
		static void SubmitSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& color, float intensity,
			float range, float innerCutoffDegrees, float outerCutoffDegrees, bool castsShadows = false, float shadowSoftness = 0.5f,
			float depthBias = 1.0f);

		static void SubmitEnvironment(AssetHandle environmentMap, float intensity, float rotationDegrees);

		// Per-frame counters, reset at the top of EndScene. Read by the editor ProfilerPanel.
		struct Statistics
		{
			uint32_t DrawCalls = 0; // every draw call, all passes
			uint32_t ShadowDrawCalls = 0; // sun cascades + spot + point faces (subset of DrawCalls)

			uint32_t OpaqueMeshes = 0; // forward opaque/mask submissions
			uint32_t BlendedMeshes = 0; // forward blended submissions
			uint32_t TriangleCount = 0; // forward pass
			uint32_t IndexCount = 0; // forward pass

			uint32_t MaterialBinds = 0; // material state switches (lower = better batching)

			uint32_t DirectionalLights = 0;
			uint32_t PointLights = 0;
			uint32_t SpotLights = 0;
			uint32_t ShadowCasters = 0; // shadow-casting lights: sun + spot + point

			uint32_t CascadeCount = 0; // active sun cascades this frame
			uint32_t PointShadowCubes = 0; // point-shadow cube maps rendered

			uint32_t ForwardDrawCalls() const { return OpaqueMeshes + BlendedMeshes; }
		};
		static Statistics& GetStats();
		static void ResetStats();
	};
}
