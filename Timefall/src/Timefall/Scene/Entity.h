#pragma once

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Components.h"

#include <entt.hpp>

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
			T& component = m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
			m_Scene->OnComponentAdded<T>(*this, component);

			return component;
		}
		
		template<typename T, typename... Args>
		T& AddOrReplaceComponent(Args&&... args)
		{
			T& component = m_Scene->m_Registry.emplace_or_replace	<T>(m_EntityHandle, std::forward<Args>(args)...);
			m_Scene->OnComponentAdded<T>(*this, component);

			return component;
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
		operator uint32_t() const { return (uint32_t)m_EntityHandle; }
		operator entt::entity() const { return m_EntityHandle; }

		UUID GetUUID()
		{
			TF_CORE_ASSERT(HasComponent<IDComponent>(), "Entity does not have a UUID component!");
			return GetComponent<IDComponent>().ID;
		}

		const std::string& GetName()
		{
			TF_CORE_ASSERT(HasComponent<TagComponent>(), "Entity does not have a Tag component!");
			return GetComponent<TagComponent>().Tag;
		}
		
		// TODO: Remove
		Scene* GetScene() const { return m_Scene; }

		bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
		bool operator!=(const Entity& other) const { return !(*this == other); }

	private:
		entt::entity m_EntityHandle{ entt::null };
		Scene* m_Scene = nullptr;

		friend class SceneSerializer;
	};
}
