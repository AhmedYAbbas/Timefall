#pragma once

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Components.h"

#include <entt.hpp>

namespace Timefall
{
	class TF_API Entity
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

		// True only if this handle refers to a LIVE entity in its scene's registry. Necessary
		// because entt recycles ids (index + version): a destroyed entity's handle is non-null
		// but stale, so operator bool's != null check cannot detect it. Use this before touching
		// components on a handle that may have been destroyed (e.g. a cached editor selection).
		bool IsValid() const { return m_Scene && m_Scene->m_Registry.valid(m_EntityHandle); }

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

		// --- Scene-graph world transform ---------------------------------------------------------
		// Resolved by walking the parent chain (RelationshipComponent); a root entity's world == local.
		glm::mat4 GetWorldTransform();
		void SetWorldTransform(const glm::mat4& worldTransform);

		glm::vec3 GetWorldTranslation();
		void SetWorldTranslation(const glm::vec3& translation);

		glm::vec3 GetWorldRotation();              // Euler radians
		void SetWorldRotation(const glm::vec3& rotation);

		glm::vec3 GetWorldScale();                 // read-only (lossyScale): no setter by design

		// World transform of this entity's parent (identity if it has no parent). Helper for the above.
		glm::mat4 GetParentWorldTransform();

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
