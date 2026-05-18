#include "tfpch.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Scripting/ScriptGlue.h"

#include "FileWatch.h"
#include "Timefall/Core/Application.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"

#include <filesystem>

#include <hostfxr/hostfxr.h>
#include <hostfxr/coreclr_delegates.h>
#include <hostfxr/nethost.h>

namespace Timefall
{
	static std::unordered_map<std::wstring, ScriptFieldType> s_ScriptFieldTypeMap =
	{
		{ L"System.Single",    Timefall::ScriptFieldType::Float },
		{ L"System.Double",    Timefall::ScriptFieldType::Double },
		{ L"System.Boolean",   Timefall::ScriptFieldType::Bool },
		{ L"System.Char",      Timefall::ScriptFieldType::Char },
		{ L"System.SByte",     Timefall::ScriptFieldType::SByte },
		{ L"System.Int16",     Timefall::ScriptFieldType::Int16 },
		{ L"System.Int32",     Timefall::ScriptFieldType::Int32 },
		{ L"System.Int64",     Timefall::ScriptFieldType::Int64 },
		{ L"System.Byte",      Timefall::ScriptFieldType::Byte },
		{ L"System.UInt16",    Timefall::ScriptFieldType::UInt16 },
		{ L"System.UInt32",    Timefall::ScriptFieldType::UInt32 },
		{ L"System.UInt64",    Timefall::ScriptFieldType::UInt64 },

		{ L"Timefall.Vector2", Timefall::ScriptFieldType::Vector2 },
		{ L"Timefall.Vector3", Timefall::ScriptFieldType::Vector3 },
		{ L"Timefall.Vector4", Timefall::ScriptFieldType::Vector4 },

		{ L"Timefall.Entity",  Timefall::ScriptFieldType::Entity }
	};

	namespace Utils
	{
		ScriptFieldType DotNetTypeToScriptFieldType(const std::wstring& typeName)
		{
			if (s_ScriptFieldTypeMap.contains(typeName))
				return s_ScriptFieldTypeMap[typeName];

			return ScriptFieldType::None;
		}
	}

	static constexpr const wchar_t* SCRIPTS_PATH = L"Resources\\Scripts\\";
	static constexpr const wchar_t* SCRIPT_CORE_DLL_PATH = L"Resources\\Scripts\\Timefall-ScriptCore.dll";
	static constexpr const wchar_t* SCRIPT_APP_DLL_PATH = L"Resources\\Scripts\\Sandbox.dll";
	static constexpr const char* SCRIPT_CORE_RUNTIME_CONFIG_PATH = "Resources/Scripts/Timefall-ScriptCore.runtimeconfig.json";

	// Copy a DLL to %TEMP%\Timefall\ and return the shadow path.
	// This keeps the original file unlocked so dotnet build can overwrite it while the editor runs.
	static std::wstring ShadowCopyDll(const wchar_t* sourcePath)
	{
		std::filesystem::path src(sourcePath);
		std::filesystem::path tempDir = std::filesystem::temp_directory_path() / L"Timefall";
		std::filesystem::create_directories(tempDir);
		std::filesystem::path dst = tempDir / src.filename();
		std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
		return dst.wstring();
	}

	struct ScriptEngineData
	{
		hostfxr_close_fn CloseFn = nullptr;
		hostfxr_initialize_for_runtime_config_fn InitializeForRuntimeConfigFn = nullptr;
		hostfxr_get_runtime_delegate_fn GetRuntimeDelegateFn = nullptr;
		load_assembly_and_get_function_pointer_fn LoadAssemblyAndGetFunctionPointerFn = nullptr;

		HMODULE HostFxrLib = nullptr;
		hostfxr_handle HostFxrHandle = nullptr;

		ScriptClass EntityClass;
		ScriptClass TypeRegistryClass;

		std::unordered_map<std::wstring, Ref<ScriptClass>> EntityClasses;
		std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
		std::unordered_map<UUID, ScriptFieldMap> EntityScriptFields;

		Scope<filewatch::FileWatch<std::wstring>> AppAssemblyFileWatcher;
		bool AssemblyReloadPending = false;

		// Cached invokers from base Entity class (static methods that take instance handle)
		void* (*CreateTypedInstanceFn)(const wchar_t* typeName) = nullptr;
		void* (*CreateTypedInstanceWithIDFn)(const wchar_t* typeName, UUID entityID) = nullptr;
		void (*InvokeOnCreateFn)(void* instance) = nullptr;
		void (*InvokeOnUpdateFn)(void* instance, float ts) = nullptr;
		void (*DestroyInstanceFn)(void* instance) = nullptr;
		void (*GetFieldValueFn)(void* instance, const wchar_t* fieldName, void* outValue) = nullptr;
		void (*SetFieldValueFn)(void* instance, const wchar_t* fieldName, void* value) = nullptr;

		// Runtime
		Scene* SceneContext = nullptr;
	};

	static ScriptEngineData* s_ScriptEngineData = nullptr;

	static ScriptEngineData* GetData (){
		return s_ScriptEngineData;
	}

	static void OnAppAssemblyFileSystemEvent(const std::wstring& path, const filewatch::Event change_type)
	{
		if (!s_ScriptEngineData->AssemblyReloadPending && change_type == filewatch::Event::modified)
		{
			s_ScriptEngineData->AssemblyReloadPending = true;

			Application::Get().SubmitToMainThread([]()
				{
					s_ScriptEngineData->AppAssemblyFileWatcher.reset();
					ScriptEngine::ReloadAssembly();
				});
		}
	}

	static load_assembly_and_get_function_pointer_fn LoadDotNetAssembly(const std::filesystem::path& runtimeConfigPath)
	{
		void* loadAssemblyAndGetFunctionPointer = nullptr;
		hostfxr_handle cxt = nullptr;

		int rc = s_ScriptEngineData->InitializeForRuntimeConfigFn(runtimeConfigPath.c_str(), nullptr, &cxt);
		TF_ASSERT(rc == 0, "Failed to initialize for runtime config");
		if (rc != 0 || cxt == nullptr)
		{
			if (cxt)
				s_ScriptEngineData->CloseFn(cxt);

			return nullptr;
		}

		// Save the context so we can close it later
		s_ScriptEngineData->HostFxrHandle = cxt;

		// Get the delegate for loading assemblies and getting function pointers
		rc = s_ScriptEngineData->GetRuntimeDelegateFn(cxt, hdt_load_assembly_and_get_function_pointer, &loadAssemblyAndGetFunctionPointer);
		TF_ASSERT(rc == 0, "Failed to get runtime delegate");
		if (rc != 0 || loadAssemblyAndGetFunctionPointer == nullptr)
		{
			s_ScriptEngineData->CloseFn(cxt);
			s_ScriptEngineData->HostFxrHandle = nullptr;
			return nullptr;
		}

		return (load_assembly_and_get_function_pointer_fn)loadAssemblyAndGetFunctionPointer;
	}

	void ScriptEngine::Init()
	{
		s_ScriptEngineData = new ScriptEngineData();

		bool result = LoadHostFxr();
		if (!result)
		{
			TF_CORE_ERROR("Failed to load hostfxr");
			return;
		}

		bool status = LoadAssembly(SCRIPT_CORE_RUNTIME_CONFIG_PATH);
		if (!status)
		{
			TF_CORE_ERROR("[ScriptEngine] Could not load Timefall-ScriptCore assembly.");
			return;
		}

		// Shadow-copy ScriptCore so the original file stays unlocked during editor runtime,
		// allowing dotnet build to overwrite it without needing to close the editor.
		std::wstring shadowCorePath = ShadowCopyDll(SCRIPT_CORE_DLL_PATH);

		// Base class types — not instantiated directly, but needed to cache invoker function pointers.
		s_ScriptEngineData->EntityClass = ScriptClass(shadowCorePath, L"Timefall.Entity, Timefall-ScriptCore");
		s_ScriptEngineData->TypeRegistryClass = ScriptClass(shadowCorePath, L"Timefall.TypeRegistry, Timefall-ScriptCore");

		// App assembly specific
		// App assemblies will call back into the TypeRegistry to register their entity types
		// EntityClasses relate to app assemblies, not the core assembly
		BuildTypeRegistry();

		// Cache invoker methods from base Entity class
		s_ScriptEngineData->CreateTypedInstanceFn = s_ScriptEngineData->EntityClass.GetMethod<void*(*)(const wchar_t*)>(L"CreateTypedInstance", L"Timefall.CreateTypedInstanceDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->CreateTypedInstanceWithIDFn = s_ScriptEngineData->EntityClass.GetMethod<void*(*)(const wchar_t*, UUID)>(L"CreateTypedInstanceWithID", L"Timefall.CreateTypedInstanceWithIDDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->InvokeOnCreateFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*)>(L"InvokeOnCreate", L"Timefall.InvokeOnCreateDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->InvokeOnUpdateFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*, float)>(L"InvokeOnUpdate", L"Timefall.InvokeOnUpdateDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->DestroyInstanceFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*)>(L"DestroyInstance", L"Timefall.DestroyInstanceDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->GetFieldValueFn = s_ScriptEngineData->TypeRegistryClass.GetMethod<void(*)(void*, const wchar_t*, void*)>(L"GetFieldValue", L"Timefall.GetFieldValueDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->SetFieldValueFn = s_ScriptEngineData->TypeRegistryClass.GetMethod<void(*)(void*, const wchar_t*, void*)>(L"SetFieldValue", L"Timefall.SetFieldValueDelegate, Timefall-ScriptCore");

		TF_CORE_ASSERT(s_ScriptEngineData->CreateTypedInstanceFn, "Failed to get CreateTypedInstanceParameterless from Entity class");
		TF_CORE_ASSERT(s_ScriptEngineData->CreateTypedInstanceWithIDFn, "Failed to get CreateTypedInstanceWithID from Entity class");
		TF_CORE_ASSERT(s_ScriptEngineData->InvokeOnCreateFn, "Failed to get InvokeOnCreate from Entity class");
		TF_CORE_ASSERT(s_ScriptEngineData->InvokeOnUpdateFn, "Failed to get InvokeOnUpdate from Entity class");
		TF_CORE_ASSERT(s_ScriptEngineData->DestroyInstanceFn, "Failed to get DestroyInstance from Entity class");

#if 0
		// --- Test default delegate invocation ---
		struct NativeArgs { const char* msg; int value; };
		NativeArgs a{ "Hello from native", 42 };

		auto returnValue = s_ScriptEngineData->EntityClass.InvokeMethod<int(*)(void*, int)>(L"DefaultDelegate", nullptr, &a, sizeof(a));

		TF_CORE_WARN("DefaultDelegate returned: {0}", returnValue);

		//typedef int(*default_delegate)(void*, int);
		//default_delegate defaultDelegateFunc = s_ScriptEngineData->EntityClass.GetMethod<default_delegate>(L"DefaultDelegate");
		//TF_ASSERT(defaultDelegateFunc != nullptr, "defaultDelegateFunc is null");
		//int resultA = defaultDelegateFunc(&a, sizeof(a));

		// --- Test int delegate invocation ---
		typedef void(*print_int)(int);
		auto  printIntFunc = s_ScriptEngineData->EntityClass.GetMethod<print_int>(L"PrintInt", L"Timefall.PrintIntDelegate, Timefall-ScriptCore");
		TF_ASSERT(printIntFunc != nullptr, "printIntFunc is null");
		printIntFunc(12345);

		// --- Test ints delegate invocation ---
		s_ScriptEngineData->EntityClass.InvokeMethod<void(*)(int, int)>(L"PrintInts", L"Timefall.PrintIntsDelegate, Timefall-ScriptCore", 12345, 67890);

		// --- Test string delegate invocation ---
		s_ScriptEngineData->EntityClass.InvokeMethod<void(*)(const char*)>(L"PrintString", L"Timefall.PrintStringDelegate, Timefall-ScriptCore", "Hello from C++ via ScriptClass!");

		// --- Test constructor/destructor delegate invocation ---
		void* instance = s_ScriptEngineData->EntityClass.InvokeMethod<void*(*)()>(L"CreateInstance", L"Timefall.CreateInstanceDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->EntityClass.InvokeMethod<void(*)(void*)>(L"DestroyInstance", L"Timefall.DestroyInstanceDelegate, Timefall-ScriptCore", instance);
		//////////////////////////////////////////////////////////////////////////
		void* instance2 = s_ScriptEngineData->EntityClass.InvokeMethod<void*(*)(int)>(L"CreateIntInstance", L"Timefall.CreateIntInstanceDelegate, Timefall-ScriptCore", 1753);
		s_ScriptEngineData->EntityClass.InvokeMethod<void(*)(void*)>(L"DestroyInstance", L"Timefall.DestroyInstanceDelegate, Timefall-ScriptCore", instance2);
#endif
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownHostFxr();
		delete s_ScriptEngineData;
	}

	void ScriptEngine::ReloadAssembly()
	{
		// Free GCHandles for any live instances so the managed side can unload the old ALC.
		if (s_ScriptEngineData->DestroyInstanceFn)
		{
			for (auto& [uuid, instance] : s_ScriptEngineData->EntityInstances)
			{
				if (instance && instance->m_Instance)
					s_ScriptEngineData->DestroyInstanceFn(instance->m_Instance);
			}
		}

		s_ScriptEngineData->EntityInstances.clear();
		s_ScriptEngineData->EntityScriptFields.clear();

		// The .NET runtime and ScriptCore function pointers are still valid — only the app
		// assembly (Sandbox.dll) is being reloaded. Just ask the managed TypeRegistry to
		// rebuild its registry from the new DLL on disk.
		s_ScriptEngineData->EntityClasses.clear();
		BuildTypeRegistry();
	}

	void ScriptEngine::OnRuntimeStart(Scene* scene)
	{
		s_ScriptEngineData->SceneContext = scene;
	}

	void ScriptEngine::OnRuntimeStop()
	{
		s_ScriptEngineData->SceneContext = nullptr;
		s_ScriptEngineData->EntityInstances.clear();
	}

	bool ScriptEngine::EntityClassExists(const std::wstring& moduleName)
	{
		return s_ScriptEngineData->EntityClasses.find(moduleName) != s_ScriptEngineData->EntityClasses.end();
	}

	void ScriptEngine::OnCreateEntity(Entity entity)
	{
		const auto& sc = entity.GetComponent<ScriptComponent>();
		if (ScriptEngine::EntityClassExists(sc.ModuleName))
		{
			UUID entityID = entity.GetUUID();
			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_ScriptEngineData->EntityClasses[sc.ModuleName], sc.ModuleName, entityID);
			s_ScriptEngineData->EntityInstances[entityID] = instance;

			// Copy field values from the entity's components to the script instance
			if (s_ScriptEngineData->EntityScriptFields.find(entityID) != s_ScriptEngineData->EntityScriptFields.end())
			{
				const ScriptFieldMap& fieldMap = s_ScriptEngineData->EntityScriptFields.at(entityID);
				for (const auto& [fieldName, fieldInstance] : fieldMap)
					instance->SetFieldValueRaw(fieldName, fieldInstance.m_Buffer);
			}

			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		UUID uuid = entity.GetUUID();
		if (s_ScriptEngineData->EntityInstances.find(uuid) != s_ScriptEngineData->EntityInstances.end())
		{
			s_ScriptEngineData->EntityInstances[uuid]->InvokeOnUpdate(ts);
		}
		else
		{
			TF_CORE_ERROR("Could not find ScriptInstance for entity");
		}
	}

	bool ScriptEngine::LoadAssembly(const std::filesystem::path& runtimeConfigPath)
	{
		// Point to the managed component's runtimeconfig.json produced by dotnet build/publish
		s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn = LoadDotNetAssembly(runtimeConfigPath);
		if (s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn)
			return true;

		return false;
	}

	void ScriptEngine::RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName, const wchar_t** fieldNames, const wchar_t** fieldTypeNames, int fieldCount)
	{
		// Build assembly path: Resources\Scripts\{assemblyName}.dll
		std::wstring assemblyPath = std::format(L"{}{}.dll", SCRIPTS_PATH, assemblyName);

		// Build fully qualified type name: {typeName}, {assemblyName}
		std::wstring qualifiedTypeName = std::format(L"{}, {}", typeName, assemblyName);

		Ref<ScriptClass> scriptClass = CreateRef<ScriptClass>(assemblyPath, qualifiedTypeName);

		for (int i = 0; i < fieldCount; i++)
			scriptClass->m_Fields.emplace(fieldNames[i], ScriptField{ fieldNames[i], Utils::DotNetTypeToScriptFieldType(fieldTypeNames[i]) });

		s_ScriptEngineData->EntityClasses.emplace(typeName, scriptClass);
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return s_ScriptEngineData->SceneContext;
	}

	Ref<ScriptClass> ScriptEngine::GetEntityScriptClass(const std::wstring& name)
	{
		auto it = s_ScriptEngineData->EntityClasses.find(name);
		if (it == s_ScriptEngineData->EntityClasses.end())
			return nullptr;

		return it->second;
	}

	const std::unordered_map<std::wstring, Ref<ScriptClass>>& ScriptEngine::GetEntityScriptClasses()
	{
		return s_ScriptEngineData->EntityClasses;
	}

	ScriptFieldMap& ScriptEngine::GetEntityScriptFields(Entity entity)
	{
		TF_CORE_ASSERT(entity, "Invalid entity");

		UUID entityID = entity.GetUUID();
		return s_ScriptEngineData->EntityScriptFields[entityID];
	}

	Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
	{
		auto it = s_ScriptEngineData->EntityInstances.find(entityID);
		if (it != s_ScriptEngineData->EntityInstances.end())
			return it->second;

		return nullptr;
	}

	void* ScriptEngine::GetManagedInstance(UUID uuid)
	{
		TF_CORE_ASSERT(s_ScriptEngineData->EntityInstances.find(uuid) != s_ScriptEngineData->EntityInstances.end(), "Entity does not exist");
		return s_ScriptEngineData->EntityInstances.at(uuid)->GetManagedInstance();
	}

	bool ScriptEngine::LoadHostFxr()
	{
		// Pre-allocate a large buffer for the path to hostfxr
		char_t buffer[MAX_PATH];
		size_t buffer_size = sizeof(buffer) / sizeof(char_t);
		int resultCode = get_hostfxr_path(buffer, &buffer_size, nullptr);

		TF_ASSERT(resultCode == 0, "Failed to locate hostfxr");
		if (resultCode != 0)
			return false;

		// Load hostfxr and get desired exports
		s_ScriptEngineData->HostFxrLib = LoadLibraryW(buffer);
		TF_ASSERT(s_ScriptEngineData->HostFxrLib != 0, "Failed to load hostfxr");
		if (!s_ScriptEngineData->HostFxrLib)
			return false;

		s_ScriptEngineData->InitializeForRuntimeConfigFn = (hostfxr_initialize_for_runtime_config_fn)GetProcAddress(s_ScriptEngineData->HostFxrLib, "hostfxr_initialize_for_runtime_config");
		s_ScriptEngineData->GetRuntimeDelegateFn = (hostfxr_get_runtime_delegate_fn)GetProcAddress(s_ScriptEngineData->HostFxrLib, "hostfxr_get_runtime_delegate");
		s_ScriptEngineData->CloseFn = (hostfxr_close_fn)GetProcAddress(s_ScriptEngineData->HostFxrLib, "hostfxr_close");

		return s_ScriptEngineData->InitializeForRuntimeConfigFn && s_ScriptEngineData->GetRuntimeDelegateFn && s_ScriptEngineData->CloseFn;
	}

	void ScriptEngine::ShutdownHostFxr()
	{
		// Close host context if created
		if (s_ScriptEngineData->HostFxrHandle && s_ScriptEngineData->CloseFn)
		{
			s_ScriptEngineData->CloseFn(s_ScriptEngineData->HostFxrHandle);
			s_ScriptEngineData->HostFxrHandle = nullptr;
		}

		// Unload the hostfxr library
		if (s_ScriptEngineData->HostFxrLib)
		{
			FreeLibrary(s_ScriptEngineData->HostFxrLib);
			s_ScriptEngineData->HostFxrLib = nullptr;
		}
	}

	void ScriptEngine::BuildTypeRegistry()
	{
		s_ScriptEngineData->EntityClasses.clear();
		s_ScriptEngineData->TypeRegistryClass.InvokeMethod<void(*)(const wchar_t*)>(L"BuildEntityRegistry", L"Timefall.BuildEntityRegistryDelegate, Timefall-ScriptCore", SCRIPT_APP_DLL_PATH);
		ScriptGlue::RegisterComponents();

		s_ScriptEngineData->AppAssemblyFileWatcher = CreateScope<filewatch::FileWatch<std::wstring>>(SCRIPT_APP_DLL_PATH, OnAppAssemblyFileSystemEvent);
		s_ScriptEngineData->AssemblyReloadPending = false;
	}

	bool ScriptClass::TryGetFunctionPointer(const wchar_t* methodName, const wchar_t* delegateTypeName, void** out) const
	{
		TF_ASSERT(s_ScriptEngineData, "Script engine data is not initialized");
		TF_ASSERT(s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn, "LoadAssemblyAndGetFunctionPointerFn is null");

		int rc = s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn(
			m_AssemblyPath.c_str(),
			m_TypeName.c_str(),
			methodName,
			delegateTypeName,
			nullptr,
			out
		);

		if (rc != 0)
		{
			TF_CORE_ERROR("TryGetFunctionPointer failed: assembly='{}', type='{}', method='{}', delegate='{}', rc=0x{:X}",
				std::filesystem::path(m_AssemblyPath).string(),
				std::filesystem::path(m_TypeName).string(),
				std::filesystem::path(methodName).string(),
				delegateTypeName ? std::filesystem::path(delegateTypeName).string() : "nullptr",
				static_cast<unsigned int>(rc));
		}

		return rc == 0 && out && *out != nullptr;
	}

	ScriptInstance::ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName)
		: m_ScriptClass(scriptClass)
	{
		// Create instance of the derived type using the base Entity's factory method
		TF_CORE_ASSERT(s_ScriptEngineData->CreateTypedInstanceFn, "CreateTypedInstanceFn is null");
		m_Instance = s_ScriptEngineData->CreateTypedInstanceFn(typeName.c_str());
		TF_CORE_ASSERT(m_Instance, "Failed to create script instance for type");
	}

	ScriptInstance::ScriptInstance(const Ref<ScriptClass>& scriptClass, const std::wstring& typeName, UUID entityID)
		: m_ScriptClass(scriptClass)
	{
		// Create instance of the derived type using the base Entity's factory method
		TF_CORE_ASSERT(s_ScriptEngineData->CreateTypedInstanceWithIDFn, "CreateTypedInstanceWithIDFn is null");
		m_Instance = s_ScriptEngineData->CreateTypedInstanceWithIDFn(typeName.c_str(), entityID);
		TF_CORE_ASSERT(m_Instance, "Failed to create script instance for type");
	}

	void ScriptInstance::InvokeOnCreate()
	{
		if (m_Instance && s_ScriptEngineData->InvokeOnCreateFn)
			s_ScriptEngineData->InvokeOnCreateFn(m_Instance);
	}

	void ScriptInstance::InvokeOnUpdate(float ts)
	{
		if (m_Instance && s_ScriptEngineData->InvokeOnUpdateFn)
			s_ScriptEngineData->InvokeOnUpdateFn(m_Instance, ts);
	}

	void ScriptInstance::GetFieldValueRaw(const std::wstring& fieldName, void* outValue)
	{
		if (m_Instance && s_ScriptEngineData->GetFieldValueFn)
			s_ScriptEngineData->GetFieldValueFn(m_Instance, fieldName.c_str(), outValue);
	}
	
	void ScriptInstance::SetFieldValueRaw(const std::wstring& fieldName, const void* value)
	{
		if (m_Instance && s_ScriptEngineData->SetFieldValueFn)
			s_ScriptEngineData->SetFieldValueFn(m_Instance, fieldName.c_str(), (void*)value);
	}
}