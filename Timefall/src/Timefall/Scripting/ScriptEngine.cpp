#include "tfpch.h"
#include "ScriptEngine.h"

#include <filesystem>

#include <hostfxr/hostfxr.h>
#include <hostfxr/coreclr_delegates.h>
#include <hostfxr/nethost.h>

namespace Timefall
{
	struct ScriptEngineData
	{
		hostfxr_close_fn CloseFn = nullptr;
		hostfxr_initialize_for_runtime_config_fn InitializeForRuntimeConfigFn = nullptr;
		hostfxr_get_runtime_delegate_fn GetRuntimeDelegateFn = nullptr;
		load_assembly_and_get_function_pointer_fn LoadAssemblyAndGetFunctionPointerFn = nullptr;

		HMODULE HostFxrLib = nullptr;
		hostfxr_handle HostFxrHandle = nullptr;

		ScriptClass EntityClass;
	};

	static ScriptEngineData* s_ScriptEngineData = nullptr;

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

		LoadAssembly("Resources/Scripts/Timefall-ScriptCore.runtimeconfig.json");
		s_ScriptEngineData->EntityClass = ScriptClass("Resources/Scripts/Timefall-ScriptCore.dll", L"Timefall.Entity, Timefall-ScriptCore");

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
		auto  printIntFunc = s_ScriptEngineData->EntityClass.GetMethod<print_int>(L"PrintInt", L"Timefall.PrintIntDelegate, Timefall-ScriptCore ");
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
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownHostFxr();
		delete s_ScriptEngineData;
	}

	void ScriptEngine::LoadAssembly(const std::filesystem::path& runtimeConfigPath)
	{
		// Point to the managed component's runtimeconfig.json produced by dotnet build/publish
		s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn = LoadDotNetAssembly(runtimeConfigPath);
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

	bool ScriptClass::TryGetFunctionPointer(const wchar_t* methodName, const wchar_t* delegateType, void** out) const
	{
		TF_ASSERT(s_ScriptEngineData, "Script engine data is not initialized");
		TF_ASSERT(s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn, "LoadAssemblyAndGetFunctionPointerFn is null");

		int rc = s_ScriptEngineData->LoadAssemblyAndGetFunctionPointerFn(
			m_AssemblyPath.c_str(),
			m_TypeName.c_str(),
			methodName,
			delegateType,
			nullptr,
			out
		);

		return rc == 0 && out && *out != nullptr;
	}
}