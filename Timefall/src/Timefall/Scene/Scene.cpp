#include "tfpch.h"

#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Timefall/Renderer/Renderer2D.h"

namespace Timefall
{
	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
	}

	void Scene::OnUpdate(Timestep ts)
	{
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
			Renderer2D::DrawQuad(transform, sprite.Color);
		}
	}

	Entity Scene::CreateEntity(const std::string tag)
	{
		Entity e = { m_Registry.create(), this };
		e.AddComponent<TransformComponent>();
		TagComponent& tagComp = e.AddComponent<TagComponent>();
		tagComp.Tag = tag.empty() ? "Entity" : tag;

		return e;
	}
}