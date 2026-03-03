#pragma once

#include <filesystem>
#include <string>

#include "Timefall/Core/Timestep.h"
#include "Timefall/Core/UUID.h"

namespace Timefall
{
	class Scene;
	class Entity;

	class TF_API ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static bool EntityClassExists(const std::wstring& moduleName);
		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, Timestep ts);

		static void LoadAssembly(const std::filesystem::path& runtimeConfigPath);
		static void RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName);

		static Scene* GetSceneContext();
		static const std::unordered_map<std::wstring, Ref<class ScriptClass>>& GetEntityClasses();

	private:
		static bool LoadHostFxr();
		static void ShutdownHostFxr();
	};

	class TF_API ScriptClass
	{
	public:
		ScriptClass() = default;

		ScriptClass(const std::wstring& assemblyPath, const std::wstring& typeName)
			: m_AssemblyPath(assemblyPath), m_TypeName(typeName)
		{
		}

		~ScriptClass() = default;

		inline void* Instantiate() const
		{
			return InvokeMethod<void*(*)()>(L"CreateInstance", L"Timefall.CreateInstanceDelegate, Timefall-ScriptCore");
		}

		// Typed getter (template must be in header)
		template<typename T>
		T GetMethod(const wchar_t* methodName, const wchar_t* delegateTypeName = nullptr) const
		{
			void* fn = nullptr;
			if (!TryGetFunctionPointer(methodName, delegateTypeName, &fn))
				return reinterpret_cast<T>(nullptr);

			return reinterpret_cast<T>(fn);
		}

		// Invoke helper that casts and calls the function pointer.
		// Usage:
		//   myClass.Invoke<void(*)(int)>(L"PrintInt", L"MyNamespace.PrintIntDelegate, Timefall-ScriptCore", 123);
		template<typename T, typename... Args>
		auto InvokeMethod(const wchar_t* methodName, const wchar_t* delegateTypeName, Args&&... args) const
			-> std::invoke_result_t<T, Args...>
		{
			T fn = GetMethod<T>(methodName, delegateTypeName);
			TF_ASSERT(fn != nullptr, "Managed method not found or invalid delegate type");
			return fn(std::forward<Args>(args)...);
		}

		// Accessors (if needed)
		inline const std::wstring& GetAssemblyPath() const { return m_AssemblyPath; }
		inline const std::wstring& GetTypeName() const { return m_TypeName; }
		
	private:
		bool TryGetFunctionPointer(const wchar_t* methodName, const wchar_t* delegateTypeName, void** out) const;

	private:
		std::wstring m_AssemblyPath;
		std::wstring m_TypeName;
	};

	class TF_API ScriptInstance
	{
	public:
		ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName);
		ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName, UUID entityID);

		void InvokeOnCreate();
		void InvokeOnUpdate(float ts);

	private:
		Ref<ScriptClass> m_ScriptClass;

		void* m_Instance = nullptr; // GCHandle to the managed instance
	};
}