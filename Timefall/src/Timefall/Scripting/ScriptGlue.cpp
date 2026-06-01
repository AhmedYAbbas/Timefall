#include "tfpch.h"
#include "Timefall/Scripting/ScriptGlue.h"

#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Core/UUID.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"

#include <glm/gtx/io.hpp>
#include <box2d/types.h>
#include <box2d/box2d.h>

namespace Timefall
{
	static std::unordered_map<std::string, std::function<bool(Entity)>> s_EntityHasComponentFuncs;
	static std::unordered_map<std::string, std::function<void(Entity)>> s_EntityAddComponentFuncs;

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

			TF_CORE_ASSERT(s_EntityHasComponentFuncs.find(componentTypeFullName) != s_EntityHasComponentFuncs.end(), "Component type not registered");
			return s_EntityHasComponentFuncs[componentTypeFullName](entity);
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
			TF_CORE_ASSERT(it != s_EntityAddComponentFuncs.end(), "Component type not registered for AddComponent");
			it->second(entity);
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
			return Input::IsKeyPressed(keycode);
		}

		__declspec(dllexport)
		void Native_RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName, const wchar_t** fieldNames, const wchar_t** fieldTypeNames, int fieldCount)
		{
			ScriptEngine::RegisterEntityTypes(typeName, assemblyName, fieldNames, fieldTypeNames, fieldCount);
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