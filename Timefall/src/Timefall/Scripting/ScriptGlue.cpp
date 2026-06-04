#include "tfpch.h"
#include "Timefall/Scripting/ScriptGlue.h"

#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Core/UUID.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"
#include "Timefall/Scene/SceneManager.h"

#include <glm/gtx/io.hpp>
#include <box2d/types.h>
#include <box2d/box2d.h>

namespace Timefall
{
	static std::unordered_map<std::string, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
	static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityAddComponentFuncs;
	static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityRemoveComponentFuncs;
	static std::unordered_map<std::string, std::function<void(Scene*, std::vector<UUID>&)>> s_EntityGetEntitiesWithFuncs;

	// Built-in component/field names cross the boundary as ASCII identifiers; widen byte-wise.
	static std::wstring WidenAscii(const char* s)
	{
		return s ? std::wstring(s, s + std::strlen(s)) : std::wstring();
	}

	// Collects the UUIDs of all entities in `scene` that have the named component. Built-in types
	// dispatch through the enumerator map (entt view); user types fall back to a ManagedComponentStorage scan.
	static std::vector<UUID> CollectEntitiesWithComponent(Scene* scene, const char* typeName)
	{
		std::vector<UUID> result;

		auto it = s_EntityGetEntitiesWithFuncs.find(typeName);
		if (it != s_EntityGetEntitiesWithFuncs.end())
		{
			it->second(scene, result);
			return result;
		}

		// User-defined component: scan the managed storage.
		std::wstring wide = WidenAscii(typeName);
		for (auto e : scene->GetAllEntitiesWithUsingView<ManagedComponentStorage>())
		{
			Entity entity{ e, scene };
			if (entity.GetComponent<ManagedComponentStorage>().Components.count(wide))
				result.push_back(entity.GetUUID());
		}

		return result;
	}

	extern "C"
	{
		__declspec(dllexport)
		void NativeLog(const char* str, int parameter)
		{
			std::cout << str << ", " << parameter << '\n';
		}

		__declspec(dllexport)
		void NativeLog_Vector(glm::vec3* vec, glm::vec3* outResult)
		{
			std::cout << *vec << '\n';

			*outResult = glm::normalize(*vec);
		}

		__declspec(dllexport)
		float NativeLog_VectorDot(glm::vec3* vec)
		{
			std::cout << *vec << '\n';
			return glm::dot(*vec, *vec);
		}

		__declspec(dllexport)
		void* GetScriptInstance(UUID entityID)
		{
			return ScriptEngine::GetManagedInstance(entityID);
		}

		__declspec(dllexport)
		bool Entity_HasComponent(UUID entityID, const char* componentTypeFullName)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			auto it = s_EntityHasComponentFuncs.find(componentTypeFullName);
			if (it != s_EntityHasComponentFuncs.end())
				return it->second(entity);

			// User-defined component: look it up in the managed storage.
			if (!entity.HasComponent<ManagedComponentStorage>())
				return false;
			const auto& storage = entity.GetComponent<ManagedComponentStorage>();
			return storage.Components.find(WidenAscii(componentTypeFullName)) != storage.Components.end();
		}

		__declspec(dllexport)
		uint64_t Entity_FindEntityByName(const char* name)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->FindEntityByName(name);

			if (!entity)
				return 0;

			return entity.GetUUID();
		}

		__declspec(dllexport)
		uint64_t Scene_CreateEntity(const char* name)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");

			Entity entity = scene->CreateEntity(name ? name : "Entity");
			return entity.GetUUID();
		}

		__declspec(dllexport)
		void Entity_Destroy(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");

			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity)
			{
				TF_CORE_ERROR("Entity_Destroy: entity {0} not found", (uint64_t)entityID);
				return;
			}

			// Deferred: flushed at the end of the runtime update so it's safe to call
			// from within a script's OnUpdate (e.g. line clears destroying many cells).
			scene->SubmitToDestroyEntity(entity);
		}

		__declspec(dllexport)
		void Entity_AddComponent(UUID entityID, const char* componentTypeFullName)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			auto it = s_EntityAddComponentFuncs.find(componentTypeFullName);
			if (it != s_EntityAddComponentFuncs.end())
			{
				it->second(entity);
				return;
			}

			// User-defined component: ensure storage + default-init fields from the schema.
			ScriptEngine::AddManagedComponent(entity, WidenAscii(componentTypeFullName));
		}

		__declspec(dllexport)
		void Entity_RemoveComponent(UUID entityID, const char* componentTypeFullName)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			auto it = s_EntityRemoveComponentFuncs.find(componentTypeFullName);
			if (it != s_EntityRemoveComponentFuncs.end())
			{
				it->second(entity);
				return;
			}

			// User-defined component: erase from the managed storage.
			if (!entity.HasComponent<ManagedComponentStorage>())
				return;
			auto& storage = entity.GetComponent<ManagedComponentStorage>();
			storage.Components.erase(WidenAscii(componentTypeFullName));
		}

		__declspec(dllexport)
		void ManagedComponent_GetField(UUID entityID, const wchar_t* typeName, const wchar_t* fieldName, void* outValue, int size)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity || !entity.HasComponent<ManagedComponentStorage>())
				return;

			auto& storage = entity.GetComponent<ManagedComponentStorage>();
			auto compIt = storage.Components.find(typeName);
			if (compIt == storage.Components.end())
				return;

			auto fieldIt = compIt->second.Fields.find(fieldName);
			if (fieldIt == compIt->second.Fields.end())
				return;

			// Copy exactly the caller's type size (clamped to the 16-byte buffer). The C# side
			// passes sizeof(T); a fixed 16-byte copy would overrun a smaller dest (e.g. int).
			size_t n = (size_t)size < ScriptFieldInstance::BufferSize() ? (size_t)size : ScriptFieldInstance::BufferSize();
			memcpy(outValue, fieldIt->second.Data(), n);
		}

		__declspec(dllexport)
		void ManagedComponent_SetField(UUID entityID, const wchar_t* typeName, const wchar_t* fieldName, void* value, int size)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity || !entity.HasComponent<ManagedComponentStorage>())
				return;

			auto& storage = entity.GetComponent<ManagedComponentStorage>();
			auto compIt = storage.Components.find(typeName);
			if (compIt == storage.Components.end())
				return;

			auto fieldIt = compIt->second.Fields.find(fieldName);
			if (fieldIt == compIt->second.Fields.end())
				return;

			size_t n = (size_t)size < ScriptFieldInstance::BufferSize() ? (size_t)size : ScriptFieldInstance::BufferSize();
			memcpy(fieldIt->second.Data(), value, n);
		}

		__declspec(dllexport)
		void SpriteRendererComponent_GetColor(UUID entityID, glm::vec4* outColor)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<SpriteRendererComponent>(), "Entity does not have a sprite renderer component!");

			*outColor = entity.GetComponent<SpriteRendererComponent>().Color;
		}

		__declspec(dllexport)
		void SpriteRendererComponent_SetColor(UUID entityID, glm::vec4* color)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<SpriteRendererComponent>(), "Entity does not have a sprite renderer component!");

			entity.GetComponent<SpriteRendererComponent>().Color = *color;
		}

		__declspec(dllexport)
		void TransformComponent_GetTranslation(UUID entityID, glm::vec3* outTranslation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outTranslation = entity.GetComponent<TransformComponent>().Translation;
		}

		__declspec(dllexport)
		void TransformComponent_SetTranslation(UUID entityID, glm::vec3* translation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			entity.GetComponent<TransformComponent>().Translation = *translation;
		}

		__declspec(dllexport)
		void TransformComponent_GetRotation(UUID entityID, glm::vec3* outRotation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outRotation = entity.GetComponent<TransformComponent>().Rotation; // Euler radians
		}

		__declspec(dllexport)
		void TransformComponent_SetRotation(UUID entityID, glm::vec3* rotation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			entity.GetComponent<TransformComponent>().Rotation = *rotation; // Euler radians
		}

		__declspec(dllexport)
		void TransformComponent_GetScale(UUID entityID, glm::vec3* outScale)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outScale = entity.GetComponent<TransformComponent>().Scale;
		}

		__declspec(dllexport)
		void TransformComponent_SetScale(UUID entityID, glm::vec3* scale)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			entity.GetComponent<TransformComponent>().Scale = *scale;
		}

		__declspec(dllexport)
		void TransformComponent_GetWorldTranslation(UUID entityID, glm::vec3* outTranslation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outTranslation = entity.GetWorldTranslation();
		}

		__declspec(dllexport)
		void TransformComponent_SetWorldTranslation(UUID entityID, glm::vec3* translation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			entity.SetWorldTranslation(*translation);
		}

		__declspec(dllexport)
		void TransformComponent_GetWorldRotation(UUID entityID, glm::vec3* outRotation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outRotation = entity.GetWorldRotation(); // Euler radians
		}

		__declspec(dllexport)
		void TransformComponent_SetWorldRotation(UUID entityID, glm::vec3* rotation)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			entity.SetWorldRotation(*rotation); // Euler radians
		}

		__declspec(dllexport)
		void TransformComponent_GetWorldScale(UUID entityID, glm::vec3* outScale)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			*outScale = entity.GetWorldScale(); // read-only lossy scale
		}

		__declspec(dllexport)
		void Entity_SetParent(UUID childID, UUID parentID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity child = scene->GetEntityByUUID(childID);
			TF_CORE_ASSERT(child, "Entity is null");

			Entity parent = parentID != 0 ? scene->GetEntityByUUID(parentID) : Entity{};
			scene->SetParent(child, parent);
		}

		__declspec(dllexport)
		uint64_t Entity_GetParent(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity || !entity.HasComponent<RelationshipComponent>())
				return 0;

			return entity.GetComponent<RelationshipComponent>().Parent;
		}

		__declspec(dllexport)
		int Entity_GetChildrenCount(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity || !entity.HasComponent<RelationshipComponent>())
				return 0;

			return (int)entity.GetComponent<RelationshipComponent>().Children.size();
		}

		__declspec(dllexport)
		void Entity_GetChildren(UUID entityID, uint64_t* outChildren, int count)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			if (!entity || !entity.HasComponent<RelationshipComponent>())
				return;

			const auto& children = entity.GetComponent<RelationshipComponent>().Children;
			int n = count < (int)children.size() ? count : (int)children.size();
			for (int i = 0; i < n; ++i)
				outChildren[i] = children[i];
		}

		__declspec(dllexport)
		int Entity_GetEntitiesWithComponentCount(const char* componentTypeFullName)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			return (int)CollectEntitiesWithComponent(scene, componentTypeFullName).size();
		}

		__declspec(dllexport)
		void Entity_GetEntitiesWithComponent(const char* componentTypeFullName, uint64_t* outEntities, int count)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");

			// Stateless count-then-fill: re-run the query and copy up to `count` (the caller's buffer size).
			std::vector<UUID> result = CollectEntitiesWithComponent(scene, componentTypeFullName);
			int n = count < (int)result.size() ? count : (int)result.size();
			for (int i = 0; i < n; ++i)
				outEntities[i] = result[i]; // UUID -> uint64_t via implicit conversion
		}

		
		__declspec(dllexport)
		void Rigidbody2DComponent_ApplyLinearImpulse(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			auto& physicsBodiesMap = scene->GetPhysicsBodiesMap();
			if (physicsBodiesMap.find(entity) == physicsBodiesMap.end())
			{
				TF_CORE_ERROR("Entity does not have a physics body!");
				return;
			}

			b2BodyId body = physicsBodiesMap[entity];
			b2Body_ApplyLinearImpulse(body, b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
		}
		
		__declspec(dllexport)
		void Rigidbody2DComponent_ApplyLinearImpulseToCenter(UUID entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			auto& physicsBodiesMap = scene->GetPhysicsBodiesMap();
			if (physicsBodiesMap.find(entity) == physicsBodiesMap.end())
			{
				TF_CORE_ERROR("Entity does not have a physics body!");
				return;
			}

			b2BodyId body = physicsBodiesMap[entity];
			b2Body_ApplyLinearImpulseToCenter(body, b2Vec2(impulse->x, impulse->y), wake);
		}

		__declspec(dllexport)
		void Rigidbody2DComponent_GetLinearVelocity(UUID entityID, glm::vec2* outLinearVelocity)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (scene->GetPhysicsBodiesMap().find(entity) == scene->GetPhysicsBodiesMap().end())
			{
				TF_CORE_ERROR("Entity does not have a physics body!");
				return;
			}

			b2BodyId body = scene->GetPhysicsBodiesMap()[entity];
			const b2Vec2& linearVelocity = b2Body_GetLinearVelocity(body);
			*outLinearVelocity = glm::vec2(linearVelocity.x, linearVelocity.y);
		}

		__declspec(dllexport)
		Rigidbody2DComponent::BodyType Rigidbody2DComponent_GetBodyType(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (scene->GetPhysicsBodiesMap().find(entity) == scene->GetPhysicsBodiesMap().end())
			{
				TF_CORE_ERROR("Entity does not have a physics body!");
				return Rigidbody2DComponent::BodyType::Static;
			}

			b2BodyId body = scene->GetPhysicsBodiesMap()[entity];
			return (Rigidbody2DComponent::BodyType)b2Body_GetType(body);
		}

		__declspec(dllexport)
		void Rigidbody2DComponent_SetBodyType(UUID entityID, Rigidbody2DComponent::BodyType bodyType)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (scene->GetPhysicsBodiesMap().find(entity) == scene->GetPhysicsBodiesMap().end())
			{
				TF_CORE_ERROR("Entity does not have a physics body!");
				return;
			}

			b2BodyId body = scene->GetPhysicsBodiesMap()[entity];
			b2Body_SetType(body, (b2BodyType)bodyType);
		}

		__declspec(dllexport)
		const char* TextComponent_GetText(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (!entity.HasComponent<TextComponent>())
			{
				TF_CORE_ERROR("Entity does not have a text component!");
				return "";
			}

			auto& tc = entity.GetComponent<TextComponent>();
			return tc.Text.c_str();
		}

		__declspec(dllexport)
		void TextComponent_SetText(UUID entityID, const char* text)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<TextComponent>(), "Entity does not have a text component!");

			auto& tc = entity.GetComponent<TextComponent>();
			tc.Text = text;
		}

		__declspec(dllexport)
		void TextComponent_GetColor(UUID entityID, glm::vec4* color)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<TextComponent>(), "Entity does not have a text component!");

			if (!entity.HasComponent<TextComponent>())
			{
				TF_CORE_ERROR("Entity does not have a text component!");
				*color = glm::vec4(0.0f);
				return;
			}

			auto& tc = entity.GetComponent<TextComponent>();
			*color = tc.Color;
		}

		__declspec(dllexport)
		void TextComponent_SetColor(UUID entityID, glm::vec4* color)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<TextComponent>(), "Entity does not have a text component!");

			auto& tc = entity.GetComponent<TextComponent>();
			tc.Color = *color;
		}

		__declspec(dllexport)
		float TextComponent_GetKerning(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (!entity.HasComponent<TextComponent>())
			{
				TF_CORE_ERROR("Entity does not have a text component!");
				return 0.0f;
			}

			auto& tc = entity.GetComponent<TextComponent>();
			return tc.Kerning;
		}

		__declspec(dllexport)
		void TextComponent_SetKerning(UUID entityID, float kerning)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<TextComponent>(), "Entity does not have a text component!");

			auto& tc = entity.GetComponent<TextComponent>();
			tc.Kerning = kerning;
		}

		__declspec(dllexport)
		float TextComponent_GetLineSpacing(UUID entityID)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");

			if (!entity.HasComponent<TextComponent>())
			{
				TF_CORE_ERROR("Entity does not have a text component!");
				return 0.0f;
			}

			auto& tc = entity.GetComponent<TextComponent>();
			return tc.LineSpacing;
		}

		__declspec(dllexport)
		void TextComponent_SetLineSpacing(UUID entityID, float lineSpacing)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			TF_CORE_ASSERT(scene, "Scene context is null");
			Entity entity = scene->GetEntityByUUID(entityID);
			TF_CORE_ASSERT(entity, "Entity is null");
			TF_CORE_ASSERT(entity.HasComponent<TextComponent>(), "Entity does not have a text component!");

			auto& tc = entity.GetComponent<TextComponent>();
			tc.LineSpacing = lineSpacing;
		}

		__declspec(dllexport)
		bool Input_IsKeyDown(KeyCode keycode)
		{
			return Input::IsKeyDown(keycode);
		}

		__declspec(dllexport)
		bool Input_IsKeyPressedThisFrame(KeyCode keycode)
		{
			return Input::IsKeyPressedThisFrame(keycode);
		}

		__declspec(dllexport)
		bool Input_IsKeyReleasedThisFrame(KeyCode keycode)
		{
			return Input::IsKeyReleasedThisFrame(keycode);
		}

		__declspec(dllexport)
		bool Input_IsMouseButtonDown(MouseCode button)
		{
			return Input::IsMouseButtonDown(button);
		}

		__declspec(dllexport)
		bool Input_IsMouseButtonPressedThisFrame(MouseCode button)
		{
			return Input::IsMouseButtonPressedThisFrame(button);
		}

		__declspec(dllexport)
		bool Input_IsMouseButtonReleasedThisFrame(MouseCode button)
		{
			return Input::IsMouseButtonReleasedThisFrame(button);
		}

		__declspec(dllexport)
		void Input_GetMousePosition(glm::vec2* outPosition)
		{
			*outPosition = Input::GetViewportMousePosition();
		}

		__declspec(dllexport)
		void Input_GetMouseWorldPosition(glm::vec2* outPosition)
		{
			Scene* scene = ScriptEngine::GetSceneContext();
			if (!scene)
			{
				*outPosition = glm::vec2(0.0f);
				return;
			}
			*outPosition = scene->ScreenToWorldPoint(Input::GetViewportMousePosition());
		}

		__declspec(dllexport)
		void Native_RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName, const wchar_t** fieldNames, const wchar_t** fieldTypeNames, int fieldCount)
		{
			ScriptEngine::RegisterEntityTypes(typeName, assemblyName, fieldNames, fieldTypeNames, fieldCount);
		}

		__declspec(dllexport)
		void Native_RegisterComponentTypes(const wchar_t* typeName, const wchar_t* assemblyName, const wchar_t** fieldNames, const wchar_t** fieldTypeNames, int fieldCount)
		{
			ScriptEngine::RegisterComponentTypes(typeName, assemblyName, fieldNames, fieldTypeNames, fieldCount);
		}

		__declspec(dllexport)
		void SceneManager_LoadScene(const char* name)
		{
			SceneManager::LoadScene(name ? name : "");
		}
	}

	template<typename... T>
	static void RegisterComponent()
	{
		([]()
		{
			std::string_view typeName = typeid(T).name();
			size_t pos = typeName.find_last_of(':'); // Assume all component types are in the Timefall namespace, so we can trim the namespace from the type name
			std::string_view componentStructName = typeName.substr(pos + 1);
			std::string managedComponentTypeName = std::format("Timefall.{}", componentStructName);
			// TODO: Need to check if managedComponentTypeName actually exists in the loaded assemblies
			s_EntityHasComponentFuncs[managedComponentTypeName] = [](Entity entity) { return entity.HasComponent<T>(); };
			s_EntityAddComponentFuncs[managedComponentTypeName] = [](Entity entity) { entity.AddComponent<T>(); };
			s_EntityRemoveComponentFuncs[managedComponentTypeName] = [](Entity entity) { if (entity.HasComponent<T>()) entity.RemoveComponent<T>(); };
			s_EntityGetEntitiesWithFuncs[managedComponentTypeName] = [](Scene* scene, std::vector<UUID>& out) {
				for (auto e : scene->GetAllEntitiesWithUsingView<T>())
				{
					Entity entity{ e, scene };
					out.push_back(entity.GetUUID());
				}
			};
		}(), ...);
	}

	template<typename... T>
	static void RegisterComponent(ComponentGroup<T...>)
	{
		RegisterComponent<T...>();
	}

	void ScriptGlue::RegisterComponents()
	{
		RegisterComponent(AllComponents{});
	}
}