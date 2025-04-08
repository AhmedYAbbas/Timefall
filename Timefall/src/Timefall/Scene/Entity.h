#pragma once

#include "entt.hpp"

#include "Scene.h"

namespace Timefall
{
	class Entity
	{
	public:
		Entity() = default;
		Entity(const Entity& other) = default;
		Entity(entt::entity entity, Scene* scene);

		template<typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.any_of<T>(m_EntityHandle);
		}

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			TF_CORE_ASSERT(!HasComponent<T>(), "Entity already has the component you're trying to add!");
			return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		template<typename T>
		T& GetComponent()
		{
			TF_CORE_ASSERT(HasComponent<T>(), "Entity does not have the component you're trying to retrieve!");
			return m_Scene->m_Registry.get<T>(m_EntityHandle);
		}

		template<typename T>
		void RemoveComponent()
		{
			TF_CORE_ASSERT(HasComponent<T>(), "Entity does not have the component you're trying to remove!");
			m_Scene->m_Registry.remove<T>(m_EntityHandle);
		}

		operator bool() const { return m_EntityHandle != entt::null; }

	private:
		entt::entity m_EntityHandle{ 0 };
		Scene* m_Scene = nullptr;
	};
}