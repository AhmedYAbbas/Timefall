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
		bool Input_IsKeyDown(KeyCode keycode)
		{
			return Input::IsKeyPressed(keycode);
		}

		__declspec(dllexport)
		void Native_RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName)
		{
			ScriptEngine::RegisterEntityTypes(typeName, assemblyName);
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