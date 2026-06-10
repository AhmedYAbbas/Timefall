#include "tfpch.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"
#include "Timefall/Scene/ScriptableEntity.h"
#include "Timefall/Scene/Components.h"
#include "Timefall/Renderer/Renderer2D.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Timefall/Renderer/RenderCommand.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Math/Math.h"

#include <box2d/box2d.h>

namespace Timefall
{
	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
	}

	template<typename T>
	static void CopyComponent(entt::registry& dst, const entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto view = src.view<T>();
		for (auto srcEntity : view)
		{
			UUID uuid = src.get<IDComponent>(srcEntity).ID;
			TF_CORE_ASSERT(enttMap.find(uuid) != enttMap.end(), "The map does not contain an entity with the requested uuid");
			entt::entity dstEnttID = enttMap.at(uuid);

			auto& component = src.get<T>(srcEntity);
			dst.emplace_or_replace<T>(dstEnttID, component);
		}
	}

	template<typename T>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		if (src.HasComponent<T>())
			dst.AddOrReplaceComponent<T>(src.GetComponent<T>());
	}

	template<typename... Component>
	static void CopyComponentIfExists(ComponentGroup<Component...>, Entity dst, Entity src)
	{
		(CopyComponentIfExists<Component>(dst, src), ...);
	}

	Ref<Scene> Scene::Copy(const Ref<Scene>& srcScene)
	{
		Ref<Scene> dstScene = CreateRef<Scene>();

		dstScene->m_ViewportWidth = srcScene->m_ViewportWidth;
		dstScene->m_ViewportHeight = srcScene->m_ViewportHeight;

		auto& srcSceneRegistry = srcScene->m_Registry;
		auto& dstSceneRegistry = dstScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene
		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto e : idView)
		{
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity newEntity = dstScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		// Copy preserves UUIDs (CreateEntityWithUUID), so RelationshipComponent's Parent/Children
		// UUIDs resolve correctly in the destination scene without any remapping.
		CopyComponent<RelationshipComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<MeshComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<LightComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<TextComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ManagedComponentStorage>(dstSceneRegistry, srcSceneRegistry, enttMap);

		return dstScene;
	}

	void Scene::OnRuntimeStart()
	{
		m_IsRunning = true;

		OnPhysics2DStart();

		// Scripting
		{
			ScriptEngine::OnRuntimeStart(this);

			auto view = m_Registry.view<ScriptComponent>();
			for (auto e : view)
			{
				Entity entity{ e, this };
				ScriptEngine::OnCreateEntity(entity);
			}
		}
	}

	void Scene::OnRuntimeStop()
	{
		m_IsRunning = false;

		OnPhysics2DStop();

		ScriptEngine::OnRuntimeStop();
	}

	void Scene::OnSimulationStart()
	{
		OnPhysics2DStart();
	}

	void Scene::OnSimulationStop()
	{
		OnPhysics2DStop();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		if (!m_IsPaused || m_StepFrames-- > 0)
		{
			// Update scripts
			{
				// C# Entity OnUpdate
				{
					auto view = m_Registry.view<ScriptComponent>();
					for (auto e : view)
					{
						Entity entity{ e, this };
						ScriptEngine::OnUpdateEntity(entity, ts);
					}
				}

				m_Registry.view<NativeScriptComponent>().each([=](auto entity, NativeScriptComponent& nsc)
					{
						if (!nsc)
						{
							nsc.Instance = nsc.InstantiateScript();
							nsc.Instance->m_Entity = Entity{ entity, this };
							nsc.Instance->OnCreate();
						}

						nsc.Instance->OnUpdate(ts);
					});
			}

			// Physics
			{
				b2World_Step(m_PhysicsWorld, m_PhysicsTimeStep, m_PhysicsSubStepCount);

				// Retrieve transform from Box2D
				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					if (m_PhysicsBodiesMap.contains(entity))
						SyncTransformFromBody(entity, m_PhysicsBodiesMap[entity]);
				}
			}
		}

		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		{
			auto group = m_Registry.group<CameraComponent>(entt::get<TransformComponent>);
			for (auto entity : group)
			{
				auto [transform, camera] = group.get<TransformComponent, CameraComponent>(entity);
				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = Entity{ entity, this }.GetWorldTransform();
				}
			}
		}

		if (mainCamera)
		{
			// --- 3D pass (depth-tested) ---
			RenderCommand::SetDepthTest(true);
			Renderer3D::BeginScene(*mainCamera, cameraTransform);
			// Gather lights (must precede mesh submission — meshes read the Lights UBO).
			{
				auto lightView = m_Registry.view<TransformComponent, LightComponent>();
				for (auto entity : lightView)
				{
					auto& light = lightView.get<LightComponent>(entity);
					glm::mat4 world = Entity{ entity, this }.GetWorldTransform();
					glm::vec3 position = glm::vec3(world[3]);
					glm::vec3 direction = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));

					switch (light.Type)
					{
						case LightComponent::LightType::Directional:
							Renderer3D::SubmitDirectionalLight(direction, light.Color, light.Intensity);
							break;
						case LightComponent::LightType::Point:
							Renderer3D::SubmitPointLight(position, light.Color, light.Intensity, light.Range);
							break;
						case LightComponent::LightType::Spot:
							Renderer3D::SubmitSpotLight(position, direction, light.Color, light.Intensity,
								light.Range, light.InnerCutoff, light.OuterCutoff);
							break;
					}
				}
			}
			{
				auto view = m_Registry.view<TransformComponent, MeshComponent>();
				for (auto entity : view)
				{
					auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
					Renderer3D::SubmitMesh(Entity{ entity, this }.GetWorldTransform(), Renderer3D::GetPrimitive(mesh.Type), (int)entity);
				}
			}
			Renderer3D::EndScene();

			// --- 2D overlay pass (no depth test; draws on top) ---
			RenderCommand::SetDepthTest(false);
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Draw sprites
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
					Renderer2D::DrawSprite(Entity{ entity, this }.GetWorldTransform(), sprite, (int)entity);
				}
			}

			// Draw circles
			{
				auto group = m_Registry.group<CircleRendererComponent>(entt::get<TransformComponent>);
				for (auto entity : group)
				{
					auto [crc, transform] = group.get<CircleRendererComponent, TransformComponent>(entity);
					Renderer2D::DrawCircle(Entity{ entity, this }.GetWorldTransform(), crc.Color, crc.Thickness, crc.Fade, (int)entity);
				}
			}
			
			// Draw texts
			{
				auto group = m_Registry.group<TextComponent>(entt::get<TransformComponent>);
				for (auto entity : group)
				{
					auto [textComponent, transform] = group.get<TextComponent, TransformComponent>(entity);
					Renderer2D::DrawString(textComponent.Text, textComponent.FontAsset, Entity{ entity, this }.GetWorldTransform(), {textComponent.Color, textComponent.Kerning, textComponent.LineSpacing}, (int)entity);
				}
			}

			Renderer2D::EndScene();
		}

		// Safe point: scripts and component iteration are done for this frame.
		FlushDestroyQueue();
	}

	void Scene::OnUpdateSimulation(Timestep ts, EditorCamera& camera)
	{
		if (!m_IsPaused || m_StepFrames-- > 0)
		{
			// Physics
			{
				b2World_Step(m_PhysicsWorld, m_PhysicsTimeStep, m_PhysicsSubStepCount);

				// Retrieve transform from Box2D
				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					if (m_PhysicsBodiesMap.contains(entity))
						SyncTransformFromBody(entity, m_PhysicsBodiesMap[entity]);
				}
			}
		}

		// Render
		RenderScene(camera);
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
	{
		RenderScene(camera);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (width == 0 && height == 0)
			return;

		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize non-fixed aspect ratio cameras
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& camera = view.get<CameraComponent>(entity);
			if (!camera.FixedAspectRatio)
			{
				camera.Camera.SetViewportSize(width, height);
			}
		}
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{ entity, this };
		}

		return {};
	}

	glm::vec2 Scene::ScreenToWorldPoint(const glm::vec2& viewportPixel)
	{
		Entity cameraEntity = GetPrimaryCameraEntity();
		if (!cameraEntity)
		{
			TF_CORE_WARN("ScreenToWorldPoint: no primary camera in the scene");
			return glm::vec2(0.0f);
		}

		float vpW = (float)m_ViewportWidth;
		float vpH = (float)m_ViewportHeight;
		if (vpW <= 0.0f || vpH <= 0.0f)
			return glm::vec2(0.0f);

		float ndcX = (viewportPixel.x / vpW) * 2.0f - 1.0f;
		float ndcY = 1.0f - (viewportPixel.y / vpH) * 2.0f; // flip Y: screen down -> NDC up

		const glm::mat4& projection = cameraEntity.GetComponent<CameraComponent>().Camera.GetProjection();
		glm::mat4 view = glm::inverse(cameraEntity.GetWorldTransform());
		glm::vec4 world = glm::inverse(projection * view) * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
		if (world.w != 0.0f)
			world /= world.w;

		return glm::vec2(world.x, world.y);
	}

	Entity Scene::CreateEntity(const std::string tag)
	{
		return CreateEntityWithUUID(UUID(), tag);
	}

	Entity Scene::CreateEntityWithUUID(const UUID& uuid, const std::string& tag)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		TagComponent& tagComp = entity.AddComponent<TagComponent>();
		tagComp.Tag = tag.empty() ? "Entity" : tag;

		m_EntityMap[uuid] = entity;

		return entity;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		if (!entity.IsValid())
			return;

		// Keep the (soon-to-be-orphaned) parent's Children list consistent, then tear down the
		// whole subtree rooted at this entity.
		DetachFromParent(entity);
		DestroyEntityAndChildren(entity);
	}

	void Scene::DestroyEntityAndChildren(Entity entity)
	{
		if (entity.HasComponent<RelationshipComponent>())
		{
			// Copy the child list: the recursive destroy mutates the live vector as it goes.
			std::vector<UUID> children = m_Registry.get<RelationshipComponent>(entity).Children;
			for (UUID childID : children)
			{
				Entity child = GetEntityByUUID(childID);
				if (child)
					DestroyEntityAndChildren(child);
			}
		}

		m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	void Scene::LinkChildToParent(Entity child, Entity parent)
	{
		UUID childID = child.GetUUID();
		UUID parentID = parent ? parent.GetUUID() : UUID(0);

		// Emplace both relationship components up front (get_or_emplace may reallocate the pool),
		// then re-fetch fresh references before writing — never hold a ref across an emplace.
		m_Registry.get_or_emplace<RelationshipComponent>(child);
		if (parent)
			m_Registry.get_or_emplace<RelationshipComponent>(parent);

		m_Registry.get<RelationshipComponent>(child).Parent = parentID;
		if (parent)
			m_Registry.get<RelationshipComponent>(parent).Children.push_back(childID);
	}

	void Scene::DetachFromParent(Entity child)
	{
		if (!child.HasComponent<RelationshipComponent>())
			return;

		auto& rc = m_Registry.get<RelationshipComponent>(child);
		if (rc.Parent != 0)
		{
			Entity oldParent = GetEntityByUUID(rc.Parent);
			if (oldParent && oldParent.HasComponent<RelationshipComponent>())
			{
				auto& siblings = m_Registry.get<RelationshipComponent>(oldParent).Children;
				std::erase(siblings, child.GetUUID());
			}
		}
		rc.Parent = 0;
	}

	void Scene::SetParent(Entity child, Entity parent)
	{
		if (!child.IsValid())
			return;

		if (parent)
		{
			// Reject cycles: the new parent must not be the child itself nor inside the child's subtree.
			Entity ancestor = parent;
			while (ancestor)
			{
				if (ancestor == child)
				{
					TF_CORE_WARN("Scene::SetParent ignored: would create a cycle.");
					return;
				}
				if (!ancestor.HasComponent<RelationshipComponent>())
					break;
				UUID ancestorParent = ancestor.GetComponent<RelationshipComponent>().Parent;
				if (ancestorParent == 0)
					break;
				ancestor = GetEntityByUUID(ancestorParent);
			}
		}

		// Preserve the child's world transform across the reparent.
		glm::mat4 worldBefore = child.GetWorldTransform();
		DetachFromParent(child);
		LinkChildToParent(child, parent);
		child.SetWorldTransform(worldBefore);
	}

	void Scene::SubmitToDestroyEntity(Entity entity)
	{
		if (!entity)
			return;

		m_EntitiesToDestroy.push_back(entity);
	}

	void Scene::FlushDestroyQueue()
	{
		if (m_EntitiesToDestroy.empty())
			return;

		for (entt::entity handle : m_EntitiesToDestroy)
		{
			// A handle may already be invalid if it was queued more than once,
			// or destroyed as part of a parent's recursive teardown.
			if (!m_Registry.valid(handle))
				continue;

			DestroyEntity(Entity{ handle, this });
		}

		m_EntitiesToDestroy.clear();
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		// Deep-clone the entity and its whole subtree with fresh UUIDs.
		Entity newEntity = DuplicateEntitySubtree(entity);

		// Attach the clone under the same parent as the original, keeping its local transform
		// (link-only, not world-preserving) so the duplicate overlaps the original exactly.
		if (entity.HasComponent<RelationshipComponent>())
		{
			UUID parentID = entity.GetComponent<RelationshipComponent>().Parent;
			if (parentID != 0)
			{
				Entity parent = GetEntityByUUID(parentID);
				if (parent)
					LinkChildToParent(newEntity, parent);
			}
		}

		return newEntity;
	}

	Entity Scene::DuplicateEntitySubtree(Entity src)
	{
		std::string name = src.GetName(); // Copy by value: the name reference can dangle after the registry grows.
		Entity dst = CreateEntity(name);

		CopyComponentIfExists(AllComponents{}, dst, src);

		// The blind copy duplicated src's RelationshipComponent with stale Parent/Children UUIDs;
		// reset it and rebuild the hierarchy explicitly below with fresh links.
		if (dst.HasComponent<RelationshipComponent>())
			dst.GetComponent<RelationshipComponent>() = RelationshipComponent{};

		if (src.HasComponent<RelationshipComponent>())
		{
			std::vector<UUID> children = src.GetComponent<RelationshipComponent>().Children; // copy: registry grows during recursion
			for (UUID childID : children)
			{
				Entity child = GetEntityByUUID(childID);
				if (child)
				{
					Entity childClone = DuplicateEntitySubtree(child);
					LinkChildToParent(childClone, dst);
				}
			}
		}

		return dst;
	}

	Entity Scene::FindEntityByName(const std::string_view& name)
	{
		auto view = m_Registry.view<TagComponent>();
		for (auto entity : view)
		{
			const TagComponent& tc = view.get<TagComponent>(entity);
			if (tc.Tag == name)
				return Entity{ entity, this };
		}
		return {};
	}

	Entity Scene::GetEntityByUUID(const UUID& uuid)
	{
		if (m_EntityMap.find(uuid) != m_EntityMap.end())
			return Entity{ m_EntityMap.at(uuid), this };

		return {};
	}

	void Scene::OnPhysics2DStart()
	{
		b2Vec2 gravity{ 0.0f, -9.81f };
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = gravity;
		worldDef.restitutionThreshold = m_RestitutionThreshold;
		m_PhysicsWorld = b2CreateWorld(&worldDef);

		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			// Box2D lives in world space; build the body from the entity's WORLD transform so a
			// parented body spawns in the right place / orientation / size. (world == local for roots.)
			glm::vec3 worldTranslation, worldRotation, worldScale;
			Math::DecomposeTransform(entity.GetWorldTransform(), worldTranslation, worldRotation, worldScale);

			b2BodyDef bodyDef = b2DefaultBodyDef();
			bodyDef.type = (b2BodyType)rb2d.Type;
			bodyDef.position = b2Vec2(worldTranslation.x, worldTranslation.y);
			bodyDef.rotation = b2MakeRot(worldRotation.z);
			bodyDef.fixedRotation = rb2d.FixedRotation;

			b2BodyId body = b2CreateBody(m_PhysicsWorld, &bodyDef);
			m_PhysicsBodiesMap[entity] = body;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2Polygon boxShape = b2MakeOffsetBox(bc2d.Size.x * worldScale.x, bc2d.Size.y * worldScale.y, b2Vec2(bc2d.Offset.x, bc2d.Offset.y), b2MakeRot(0));
				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = bc2d.Density;
				shapeDef.material.friction = bc2d.Friction;
				shapeDef.material.restitution = bc2d.Restitution;
				b2CreatePolygonShape(body, &shapeDef, &boxShape);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2Circle circleShape{ {cc2d.Offset.x, cc2d.Offset.y}, worldScale.x * cc2d.Radius };
				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = cc2d.Density;
				shapeDef.material.friction = cc2d.Friction;
				shapeDef.material.restitution = cc2d.Restitution;
				b2CreateCircleShape(body, &shapeDef, &circleShape);
			}
		}
	}

	void Scene::OnPhysics2DStop()
	{
		b2DestroyWorld(m_PhysicsWorld);
	}

	void Scene::SyncTransformFromBody(Entity entity, b2BodyId body)
	{
		b2Vec2 position = b2Body_GetPosition(body);
		float angle = b2Rot_GetAngle(b2Body_GetRotation(body));

		bool isParented = entity.HasComponent<RelationshipComponent>()
			&& entity.GetComponent<RelationshipComponent>().Parent != 0;

		if (isParented)
		{
			// Box2D is authoritative in WORLD space; convert its pose back into local space under
			// the parent. World scale and world Z are preserved; only XY-position and Z-rotation
			// come from physics. (A dynamic body under a script-MOVED parent is unsupported —
			// physics overwrites the parent-driven motion every frame.)
			glm::vec3 worldTranslation = entity.GetWorldTranslation();
			entity.SetWorldTranslation({ position.x, position.y, worldTranslation.z });
			entity.SetWorldRotation({ 0.0f, 0.0f, angle });
		}
		else
		{
			// Root body: world == local. Fast path, identical to pre-hierarchy behavior.
			auto& transform = entity.GetComponent<TransformComponent>();
			transform.Translation.x = position.x;
			transform.Translation.y = position.y;
			transform.Rotation.z = angle;
		}
	}

	void Scene::RenderScene(EditorCamera& camera)
	{
		// --- 3D pass (depth-tested) ---
		RenderCommand::SetDepthTest(true);
		Renderer3D::BeginScene(camera);
		// Gather lights (must precede mesh submission — meshes read the Lights UBO).
		{
			auto lightView = m_Registry.view<TransformComponent, LightComponent>();
			for (auto entity : lightView)
			{
				auto& light = lightView.get<LightComponent>(entity);
				glm::mat4 world = Entity{ entity, this }.GetWorldTransform();
				glm::vec3 position = glm::vec3(world[3]);
				glm::vec3 direction = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));

				switch (light.Type)
				{
					case LightComponent::LightType::Directional:
						Renderer3D::SubmitDirectionalLight(direction, light.Color, light.Intensity);
						break;
					case LightComponent::LightType::Point:
						Renderer3D::SubmitPointLight(position, light.Color, light.Intensity, light.Range);
						break;
					case LightComponent::LightType::Spot:
						Renderer3D::SubmitSpotLight(position, direction, light.Color, light.Intensity,
							light.Range, light.InnerCutoff, light.OuterCutoff);
						break;
				}
			}
		}
		{
			auto view = m_Registry.view<TransformComponent, MeshComponent>();
			for (auto entity : view)
			{
				auto [transform, mesh] = view.get<TransformComponent, MeshComponent>(entity);
				Renderer3D::SubmitMesh(Entity{ entity, this }.GetWorldTransform(), Renderer3D::GetPrimitive(mesh.Type), (int)entity);
			}
		}
		Renderer3D::EndScene();

		// --- 2D overlay pass (no depth test; draws on top) ---
		RenderCommand::SetDepthTest(false);
		Renderer2D::BeginScene(camera);

		// Draw sprites
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawSprite(Entity{ entity, this }.GetWorldTransform(), sprite, (int)entity);
			}
		}

		// Draw circles
		{
			auto group = m_Registry.group<CircleRendererComponent>(entt::get<TransformComponent>);
			for (auto entity : group)
			{
				auto [crc, transform] = group.get<CircleRendererComponent, TransformComponent>(entity);
				Renderer2D::DrawCircle(Entity{ entity, this }.GetWorldTransform(), crc.Color, crc.Thickness, crc.Fade, (int)entity);
			}
		}

		// Draw Texts
		{
			auto group = m_Registry.group<TextComponent>(entt::get<TransformComponent>);
			for (auto entity : group)
			{
				auto [textComponent, transform] = group.get<TextComponent, TransformComponent>(entity);
				Renderer2D::DrawString(textComponent.Text, textComponent.FontAsset, Entity{ entity, this }.GetWorldTransform(), {textComponent.Color, textComponent.Kerning, textComponent.LineSpacing}, (int)entity);
			}
		}

		Renderer2D::EndScene();
	}

	template<typename T>
	TF_API void Scene::OnComponentAdded(Entity entity, T& component)
	{
		//static_assert(false);
	}

	template<>
	TF_API void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	TF_API void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component)
	{
	}
	
	template<>
	TF_API void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<MeshComponent>(Entity entity, MeshComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<LightComponent>(Entity entity, LightComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component)
	{
	}
	
	template<>
	TF_API void Scene::OnComponentAdded<TextComponent>(Entity entity, TextComponent& component)
	{
	}

	template<>
	TF_API void Scene::OnComponentAdded<ManagedComponentStorage>(Entity entity, ManagedComponentStorage& component)
	{
	}
}