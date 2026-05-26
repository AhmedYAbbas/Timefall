#include "tfpch.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"
#include "Timefall/Scene/ScriptableEntity.h"
#include "Timefall/Scene/Components.h"
#include "Timefall/Renderer/Renderer2D.h"
#include "Timefall/Scripting/ScriptEngine.h"

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
			enttMap[uuid] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

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
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					if (m_PhysicsBodiesMap.contains(entity))
					{
						b2BodyId body = m_PhysicsBodiesMap[entity];
						const auto& position = b2Body_GetPosition(body);
						transform.Translation.x = position.x;
						transform.Translation.y = position.y;
						transform.Rotation.z = b2Rot_GetAngle(b2Body_GetRotation(body));
					}
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
					cameraTransform = transform.GetTransform();
				}
			}
		}

		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			// Draw sprites
			{
				auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
				for (auto entity : group)
				{
					auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
					Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
				}
			}

			// Draw circles
			{
				auto group = m_Registry.group<CircleRendererComponent>(entt::get<TransformComponent>);
				for (auto entity : group)
				{
					auto [crc, transform] = group.get<CircleRendererComponent, TransformComponent>(entity);
					Renderer2D::DrawCircle(transform.GetTransform(), crc.Color, crc.Thickness, crc.Fade, (int)entity);
				}
			}

			Renderer2D::EndScene();
		}
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
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					if (m_PhysicsBodiesMap.contains(entity))
					{
						b2BodyId body = m_PhysicsBodiesMap[entity];
						const auto& position = b2Body_GetPosition(body);
						transform.Translation.x = position.x;
						transform.Translation.y = position.y;
						transform.Rotation.z = b2Rot_GetAngle(b2Body_GetRotation(body));
					}
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
		m_EntityMap.erase(entity.GetUUID());
		m_Registry.destroy(entity);
	}

	Entity Scene::DuplicateEntity(Entity entity)
	{
		std::string name = entity.GetName(); // Get the name as a value because having a reference to the entity's name might lead to dangling references after duplication
		Entity newEntity = CreateEntity(name);

		// Important for scene serialization for some reason
		CopyComponentIfExists(AllComponents{}, newEntity, entity);
		return newEntity;
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
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
			b2BodyDef bodyDef = b2DefaultBodyDef();
			bodyDef.type = (b2BodyType)rb2d.Type;
			bodyDef.position = b2Vec2(transform.Translation.x, transform.Translation.y);
			bodyDef.rotation = b2MakeRot(transform.Rotation.z);
			bodyDef.fixedRotation = rb2d.FixedRotation;

			b2BodyId body = b2CreateBody(m_PhysicsWorld, &bodyDef);
			m_PhysicsBodiesMap[entity] = body;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2Polygon boxShape = b2MakeOffsetBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y, b2Vec2(bc2d.Offset.x, bc2d.Offset.y), b2MakeRot(0));
				b2ShapeDef shapeDef = b2DefaultShapeDef();
				shapeDef.density = bc2d.Density;
				shapeDef.material.friction = bc2d.Friction;
				shapeDef.material.restitution = bc2d.Restitution;
				b2CreatePolygonShape(body, &shapeDef, &boxShape);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2Circle circleShape{ {cc2d.Offset.x, cc2d.Offset.y}, transform.Scale.x * cc2d.Radius };
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

	void Scene::RenderScene(EditorCamera& camera)
	{
		Renderer2D::BeginScene(camera);

		// Draw sprites
		{
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);
				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
			}
		}

		// Draw circles
		{
			auto group = m_Registry.group<CircleRendererComponent>(entt::get<TransformComponent>);
			for (auto entity : group)
			{
				auto [crc, transform] = group.get<CircleRendererComponent, TransformComponent>(entity);
				Renderer2D::DrawCircle(transform.GetTransform(), crc.Color, crc.Thickness, crc.Fade, (int)entity);
			}
		}

		// Draw Texts
		{
			Renderer2D::DrawString("Timefall", Font::GetDefault(), glm::mat4(1.0f), glm::vec4(1.0f));
			Renderer2D::DrawString(
				R"(
				// MSDF text shader

				#type vertex
				#version 450 core

				layout(location = 0) in vec3 a_Position;
				layout(location = 1) in vec4 a_Color;
				layout(location = 2) in vec2 a_TexCoord;
				layout(location = 3) in int a_EntityID;

				layout(std140, binding = 0) uniform Camera
				{
					mat4 u_ViewProjection;
				};

				struct VertexOutput
				{
					vec4 Color;
					vec2 TexCoord;
				};

				layout (location = 0) out VertexOutput Output;
				layout (location = 2) out flat int v_EntityID;

				void main()
				{
					Output.Color = a_Color;
					Output.TexCoord = a_TexCoord;
					v_EntityID = a_EntityID;

					gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
				}

				#type fragment
				#version 450 core

				layout(location = 0) out vec4 o_Color;
				layout(location = 1) out int o_EntityID;

				struct VertexOutput
				{
					vec4 Color;
					vec2 TexCoord;
				};

				layout (location = 0) in VertexOutput Input;
				layout (location = 2) in flat int v_EntityID;

				layout (binding = 0) uniform sampler2D u_FontAtlas;

				float screenPxRange() {
					const float pxRange = 2.0; // set to distance field's pixel range
					vec2 unitRange = vec2(pxRange)/vec2(textureSize(u_FontAtlas, 0));
					vec2 screenTexSize = vec2(1.0)/fwidth(Input.TexCoord);
					return max(0.5*dot(unitRange, screenTexSize), 1.0);
				}

				float median(float r, float g, float b) {
					return max(min(r, g), min(max(r, g), b));
				}

				void main()
				{
					vec4 texColor = Input.Color * texture(u_FontAtlas, Input.TexCoord);

					vec3 msd = texture(u_FontAtlas, Input.TexCoord).rgb;
					float sd = median(msd.r, msd.g, msd.b);
					float screenPxDistance = screenPxRange()*(sd - 0.5);
					float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
					if (opacity == 0.0)
						discard;

					vec4 bgColor = vec4(0.0);
					o_Color = mix(bgColor, Input.Color, opacity);
					if (o_Color.a == 0.0)
						discard;
	
					o_EntityID = v_EntityID;
				})", Font::GetDefault(), glm::mat4(1.0f), glm::vec4(1.0f));
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
}