#pragma once

#include "Timefall/Core/UUID.h"
#include "Timefall/Core/Timestep.h"
#include "Timefall/Renderer/EditorCamera.h"

#include <entt.hpp>
#include <box2d/id.h>

namespace Timefall 
{
	class Entity;

	class Scene 
	{
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(const Ref<Scene>& srcScene);

		Entity CreateEntity(const std::string tag = "");
		Entity CreateEntityWithUUID(const UUID& uuid, const std::string tag = "");
		void DestroyEntity(Entity entity);
		void DuplicateEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateSimulation(Timestep ts, EditorCamera& camera);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnViewportResize(uint32_t width, uint32_t height);

		// TODO: Remove
		void SetRestitutionThreshold(float restitutionThreshold) { m_RestitutionThreshold = restitutionThreshold; }
		float GetRestitutionThreshold() const { return m_RestitutionThreshold; }

		Entity GetPrimaryCameraEntity();

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
		void OnComponentAdded(Entity entity, T& component);

		void OnPhysics2DStart();
		void OnPhysics2DStop();

		void RenderScene(EditorCamera& camera);

	private:
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		// Physics
		b2WorldId m_PhysicsWorld = b2_nullWorldId;
		std::unordered_map<entt::entity, b2BodyId> m_PhysicsBodiesMap;
		float m_RestitutionThreshold = 0.5f;
		float m_PhysicsTimeStep = 1.0f / 60.0f;
		int m_PhysicsSubStepCount = 4;

		friend class Entity;
		friend class SceneSerializer;
		friend class SceneHierarchyPanel;
	};
}