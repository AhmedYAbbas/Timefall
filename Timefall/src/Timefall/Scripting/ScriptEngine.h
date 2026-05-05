#pragma once

#include <filesystem>
#include <string>

#include "Timefall/Core/Timestep.h"
#include "Timefall/Core/UUID.h"

namespace Timefall
{
	enum class ScriptFieldType
	{
		None = 0,
		Float, Double,
		Bool, Char, SByte, Int16, Int32, Int64,
		Byte, UInt16, UInt32, UInt64,
		Vector2, Vector3, Vector4,
		Entity
	};

	struct ScriptField
	{
		std::wstring Name;
		ScriptFieldType Type;
	};

	// ScriptField + data storage
	struct ScriptFieldInstance
	{
		ScriptField Field;

		ScriptFieldInstance()
		{
			memset(m_Buffer, 0, sizeof(m_Buffer));
		}

		template<typename T>
		T GetValue()
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			return *(T*)m_Buffer;
		}

		template<typename T>
		void SetValue(T value)
		{
			static_assert(sizeof(T) <= 16, "Type too large!");
			memcpy(m_Buffer, &value, sizeof(T));
		}
	private:
		uint8_t m_Buffer[16];
		friend class ScriptEngine;
		friend class ScriptInstance;
	};


	using ScriptFieldMap = std::unordered_map<std::wstring, ScriptFieldInstance>; // entityID -> (fieldName -> field instance)

	class Scene;
	class Entity;

	class TF_API ScriptClass
	{
	public:
		ScriptClass() = default;

		ScriptClass(const std::wstring& assemblyPath, const std::wstring& typeName)
			: m_AssemblyPath(assemblyPath), m_TypeName(typeName)
		{}

		~ScriptClass() = default;

		inline void* Instantiate() const
		{
			return InvokeMethod<void* (*)()>(L"CreateInstance", L"Timefall.CreateInstanceDelegate, Timefall-ScriptCore");
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
		inline const std::unordered_map<std::wstring, ScriptField>& GetFields() const { return m_Fields; }

	private:
		bool TryGetFunctionPointer(const wchar_t* methodName, const wchar_t* delegateTypeName, void** out) const;

	private:
		std::wstring m_AssemblyPath;
		std::wstring m_TypeName;

		std::unordered_map<std::wstring, ScriptField> m_Fields;

		friend class ScriptEngine;
	};

	class TF_API ScriptInstance
	{
	public:
		ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName);
		ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName, UUID entityID);

		void InvokeOnCreate();
		void InvokeOnUpdate(float ts);

		Ref<ScriptClass> GetScriptClass() const { return m_ScriptClass; }
		void* GetManagedInstance() const { return m_Instance; }

		template<typename T>
		T GetFieldValue(const std::wstring& fieldName)
		{
			T value{};
			GetFieldValueRaw(fieldName, &value);
			return value;
		}
		
		template<typename T>
		void SetFieldValue(const std::wstring& fieldName, const T& value)
		{
			SetFieldValueRaw(fieldName, (void*)&value);
		}

		void GetFieldValueRaw(const std::wstring& fieldName, void* outValue);
		void SetFieldValueRaw(const std::wstring& fieldName, const void* value);

	private:
		Ref<ScriptClass> m_ScriptClass;

		void* m_Instance = nullptr; // GCHandle to the managed instance

		friend class ScriptEngine;
		friend struct ScriptFieldInstance;
	};

	class TF_API ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void ReloadAssembly();

		static void OnRuntimeStart(Scene* scene);
		static void OnRuntimeStop();

		static bool EntityClassExists(const std::wstring& moduleName);
		static void OnCreateEntity(Entity entity);
		static void OnUpdateEntity(Entity entity, Timestep ts);

		static void LoadAssembly(const std::filesystem::path& runtimeConfigPath);
		static void RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName, const wchar_t** fieldNames, const wchar_t** fieldTypeNames, int fieldCount);

		static Scene* GetSceneContext();
		static Ref<ScriptClass> GetEntityScriptClass(const std::wstring& name);
		static const std::unordered_map<std::wstring, Ref<class ScriptClass>>& GetEntityScriptClasses();
		static ScriptFieldMap& GetEntityScriptFields(Entity entity);

		static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityID);

		static void* GetManagedInstance(UUID uuid);

	private:
		static bool LoadHostFxr();
		static void ShutdownHostFxr();

		static void BuildTypeRegistry();
	};

	namespace Utils
	{
		inline const std::wstring ScriptFieldTypeToString(ScriptFieldType fieldType)
		{
			switch (fieldType)
			{
				case ScriptFieldType::None:     return L"None";
				case ScriptFieldType::Float:    return L"Float";
				case ScriptFieldType::Double:   return L"Double";
				case ScriptFieldType::Bool:     return L"Bool";
				case ScriptFieldType::Char:     return L"Char";
				case ScriptFieldType::SByte:    return L"SByte";
				case ScriptFieldType::Int16:    return L"Int16";
				case ScriptFieldType::Int32:    return L"Int32";
				case ScriptFieldType::Int64:    return L"Int64";
				case ScriptFieldType::Byte:     return L"Byte";
				case ScriptFieldType::UInt16:   return L"UInt16";
				case ScriptFieldType::UInt32:   return L"UInt32";
				case ScriptFieldType::UInt64:   return L"UInt64";
				case ScriptFieldType::Vector2:  return L"Vector2";
				case ScriptFieldType::Vector3:  return L"Vector3";
				case ScriptFieldType::Vector4:  return L"Vector4";
				case ScriptFieldType::Entity:   return L"Entity";
			}

			TF_CORE_ASSERT(false, "Unknown ScriptFieldType");
			return L"None";
		}

		inline ScriptFieldType ScriptFieldTypeFromString(const std::wstring& typeName)
		{
			if (typeName == L"None")     return ScriptFieldType::None;
			if (typeName == L"Float")    return ScriptFieldType::Float;
			if (typeName == L"Double")   return ScriptFieldType::Double;
			if (typeName == L"Bool")     return ScriptFieldType::Bool;
			if (typeName == L"Char")     return ScriptFieldType::Char;
			if (typeName == L"SByte")    return ScriptFieldType::SByte;
			if (typeName == L"Int16")    return ScriptFieldType::Int16;
			if (typeName == L"Int32")    return ScriptFieldType::Int32;
			if (typeName == L"Int64")    return ScriptFieldType::Int64;
			if (typeName == L"Byte")     return ScriptFieldType::Byte;
			if (typeName == L"UInt16")   return ScriptFieldType::UInt16;
			if (typeName == L"UInt32")   return ScriptFieldType::UInt32;
			if (typeName == L"UInt64")   return ScriptFieldType::UInt64;
			if (typeName == L"Vector2")  return ScriptFieldType::Vector2;
			if (typeName == L"Vector3")  return ScriptFieldType::Vector3;
			if (typeName == L"Vector4")  return ScriptFieldType::Vector4;
			if (typeName == L"Entity")   return ScriptFieldType::Entity;

			TF_CORE_ASSERT(false, "Unknown ScriptFieldType name");
			return ScriptFieldType::None;
		}
	}
}