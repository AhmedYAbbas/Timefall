#pragma once

#include "Timefall/Asset/Asset.h"
#include "Timefall/Core/UUID.h"
#include "Timefall/Core/Timestep.h"
#include "Timefall/Renderer/EditorCamera.h"

#include <entt.hpp>
#include <box2d/id.h>

namespace Timefall 
{
	class Entity;

	class TF_API Scene : public Asset
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(const Ref<Scene>& srcScene);

		virtual AssetType GetType() const override { return AssetType::Scene; }

		Entity CreateEntity(const std::string tag = "");
		Entity CreateEntityWithUUID(const UUID& uuid, const std::string& tag = "");
		void DestroyEntity(Entity entity);
		// Queues an entity for destruction at the end of the current runtime update,
		// so it's safe to call from within scripts / component iteration. Children are
		// destroyed recursively when the queue is flushed.
		void SubmitToDestroyEntity(Entity entity);
		Entity DuplicateEntity(Entity entity);

		Entity FindEntityByName(const std::string_view& name);
		Entity GetEntityByUUID(const UUID& uuid);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnViewportResize(uint32_t width, uint32_t height);

		// TODO: Remove
		inline void SetRestitutionThreshold(float restitutionThreshold) { m_RestitutionThreshold = restitutionThreshold; }
		inline const float GetRestitutionThreshold() const { return m_RestitutionThreshold; }
		inline std::unordered_map<entt::entity, b2BodyId>& GetPhysicsBodiesMap() { return m_PhysicsBodiesMap; }

		Entity GetPrimaryCameraEntity();

		inline bool IsRunning() const { return m_IsRunning; }
		inline bool IsPaused() const { return m_IsPaused; }

		inline void SetPaused(bool paused) { m_IsPaused = paused; }

		inline void Step(int frames = 1) { m_StepFrames = frames;}

		template<typename... Components>
		auto GetAllEntitiesWithUsingOwningGroup() 
		{
			return m_Registry.group<Components...>();
		}

		template<typename... Components>
		auto GetAllEntitiesWithUsingView() 
		{
			return m_Registry.view<Components...>();
		}

	private:
		template<typename T>
		TF_API void OnComponentAdded(Entity entity, T& component);

		void OnPhysics2DStart();
		void OnPhysics2DStop();

		// Destroys everything queued via SubmitToDestroyEntity. Called at the end of a runtime update.
		void FlushDestroyQueue();

		void RenderScene(EditorCamera& camera);

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
		bool m_IsRunning = false;
		bool m_IsPaused = false;
		int m_StepFrames = 0;

		// Physics
		b2WorldId m_PhysicsWorld = b2_nullWorldId;
		std::unordered_map<entt::entity, b2BodyId> m_PhysicsBodiesMap;
		float m_RestitutionThreshold = 0.5f;
		float m_PhysicsTimeStep = 1.0f / 60.0f;
		int m_PhysicsSubStepCount = 4;

		std::unordered_map<UUID, entt::entity> m_EntityMap;

		// Entities queued for deferred destruction (flushed at end of runtime update).
		std::vector<entt::entity> m_EntitiesToDestroy;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}