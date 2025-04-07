#pragma once

#include <entt.hpp>

#include "Timefall/Core/Timestep.h"

namespace Timefall 
{
	class Scene 
	{
	public:
		Scene();
		~Scene();

		void OnUpdate(Timestep ts);

		entt::entity CreateEntity();

		entt::registry& GetRegistry() { return m_Registry; }

	private:
		entt::registry m_Registry;
	};
}