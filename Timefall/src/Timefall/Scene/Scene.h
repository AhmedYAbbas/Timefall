#pragma once

#include <entt.hpp>

#include "Timefall/Core/Timestep.h"

namespace Timefall 
{
	class Entity;

	class Scene 
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity(const std::string tag = "");

		void OnUpdate(Timestep ts);

	private:
		entt::registry m_Registry;

		friend class Entity;
	};
}