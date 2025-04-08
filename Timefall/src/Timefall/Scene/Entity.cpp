#include "tfpch.h"
#include "Entity.h"

namespace Timefall
{
	Entity::Entity(entt::entity entity, Scene* scene)
		: m_EntityHandle(entity), m_Scene(scene)
	{
	}
}