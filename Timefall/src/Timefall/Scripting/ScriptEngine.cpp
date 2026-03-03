#include "tfpch.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/Entity.h"

#include <filesystem>

#include <hostfxr/hostfxr.h>
#include <hostfxr/coreclr_delegates.h>
#include <hostfxr/nethost.h>

namespace Timefall
{
	static constexpr const wchar_t* kScriptsPath = L"Resources\\Scripts\\";
	static constexpr const wchar_t* kScriptCoreDll = L"Resources\\Scripts\\Timefall-ScriptCore.dll";
	static constexpr const char* kScriptCoreRuntimeConfig = "Resources/Scripts/Timefall-ScriptCore.runtimeconfig.json";

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

		// Cached invokers from base Entity class (static methods that take instance handle)
		void* (*CreateTypedInstanceFn)(const wchar_t* typeName) = nullptr;
		void* (*CreateTypedInstanceWithIDFn)(const wchar_t* typeName, UUID entityID) = nullptr;
		void (*InvokeOnCreateFn)(void* instance) = nullptr;
		void (*InvokeOnUpdateFn)(void* instance, float ts) = nullptr;
		void (*DestroyInstanceFn)(void* instance) = nullptr;

		// Runtime
		Scene* SceneContext = nullptr;
	};

	static ScriptEngineData* s_ScriptEngineData = nullptr;

	static ScriptEngineData* GetData (){
		return s_ScriptEngineData;
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

		LoadAssembly(kScriptCoreRuntimeConfig);

		s_ScriptEngineData->EntityClass = ScriptClass(kScriptCoreDll, L"Timefall.Entity, Timefall-ScriptCore");
		s_ScriptEngineData->TypeRegistryClass = ScriptClass(kScriptCoreDll, L"Timefall.TypeRegistry, Timefall-ScriptCore");

		s_ScriptEngineData->EntityClasses.clear();
		s_ScriptEngineData->TypeRegistryClass.InvokeMethod<void(*)(const char*)>(L"BuildEntityRegistry", L"Timefall.BuildEntityRegistryDelegate, Timefall-ScriptCore", "Resources/Scripts/Timefall-ScriptCore.dll");

		// Cache invoker methods from base Entity class
		s_ScriptEngineData->CreateTypedInstanceFn = s_ScriptEngineData->EntityClass.GetMethod<void*(*)(const wchar_t*)>(L"CreateTypedInstance", L"Timefall.CreateTypedInstanceDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->CreateTypedInstanceWithIDFn = s_ScriptEngineData->EntityClass.GetMethod<void*(*)(const wchar_t*, UUID)>(L"CreateTypedInstanceWithID", L"Timefall.CreateTypedInstanceWithIDDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->InvokeOnCreateFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*)>(L"InvokeOnCreate", L"Timefall.InvokeOnCreateDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->InvokeOnUpdateFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*, float)>(L"InvokeOnUpdate", L"Timefall.InvokeOnUpdateDelegate, Timefall-ScriptCore");
		s_ScriptEngineData->DestroyInstanceFn = s_ScriptEngineData->EntityClass.GetMethod<void(*)(void*)>(L"DestroyInstance", L"Timefall.DestroyInstanceDelegate, Timefall-ScriptCore");

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
			UUID uuid = entity.GetUUID();
			Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(s_ScriptEngineData->EntityClasses[sc.ModuleName], sc.ModuleName, uuid);
			s_ScriptEngineData->EntityInstances[uuid] = instance;
			instance->InvokeOnCreate();
		}
	}

	void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
	{
		UUID uuid = entity.GetUUID();
		TF_CORE_ASSERT(s_ScriptEngineData->EntityInstances.find(uuid) != s_ScriptEngineData->EntityInstances.end(), "Entity does not exist");
		s_ScriptEngineData->EntityInstances[uuid]->InvokeOnUpdate(ts);
	}

	void ScriptEngine::LoadAssembly(const std::filesystem::path& runtimeConfigPath)
	{
		// Point to the managed component's runtimeconfig.json produced by dotnet build/publish
		s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn = LoadDotNetAssembly(runtimeConfigPath);
	}

	void ScriptEngine::RegisterEntityTypes(const wchar_t* typeName, const wchar_t* assemblyName)
	{
		// Build assembly path: Resources\Scripts\{assemblyName}.dll
		std::wstring assemblyPath = std::format(L"{}{}.dll", kScriptsPath, assemblyName);

		// Build fully qualified type name: {typeName}, {assemblyName}
		std::wstring qualifiedTypeName = std::format(L"{}, {}", typeName, assemblyName);

		s_ScriptEngineData->EntityClasses.emplace(typeName, CreateRef<ScriptClass>(std::move(assemblyPath), std::move(qualifiedTypeName)));
	}

	Scene* ScriptEngine::GetSceneContext()
	{
		return s_ScriptEngineData->SceneContext;
	}

	const std::unordered_map<std::wstring, Ref<class ScriptClass>>& ScriptEngine::GetEntityClasses()
	{
		return s_ScriptEngineData->EntityClasses;
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
}