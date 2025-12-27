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

		HMODULE HostFxrLib = nullptr;
		hostfxr_handle HostFxrHandle = nullptr;
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

		// Point to the managed component's runtimeconfig.json produced by dotnet build/publish
		std::filesystem::path runtime_config = "Resources/Timefall-ScriptCore.runtimeconfig.json";
		auto loader = LoadDotNetAssembly(runtime_config);

		// Prepare to load assembly and get function pointer
		//const char_t* assembly_path = L"C:/Ahmed/Dev/Timefall/Timefall-ScriptCore/bin/Debug/net9.0/Timefall-ScriptCore.dll";
		const char_t* assembly_path = L"Timefall-ScriptCore.dll";
		const char_t* type_name = L"MyNamespace.TestClass, Timefall-ScriptCore";
		

		// --- Test default delegate invocation ---
		typedef int(*default_delegate)(void*, int);
		default_delegate defaultDelegateFunc = nullptr;

		const char_t* method_name = L"DefaultDelegate";
		int rc = loader(
			assembly_path,
			type_name,
			method_name,
			nullptr,
			nullptr,
			(void**)&defaultDelegateFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(defaultDelegateFunc != nullptr, "defaultDelegateFunc is null");

		struct NativeArgs { const char* msg; int value; };
		NativeArgs a{ "Hello from native", 42 };
		int resultA = defaultDelegateFunc(&a, sizeof(a));

		// --- Test int delegate invocation ---
		typedef void(*print_int)(int);
		print_int printIntFunc = nullptr;
		method_name = L"PrintInt";
		const char_t* delegate_type = L"MyNamespace.PrintIntDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&printIntFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(printIntFunc != nullptr, "printIntFunc is null");

		printIntFunc(12345);

		// --- Test ints delegate invocation ---
		typedef void(*print_ints)(int, int);
		print_ints printIntsFunc = nullptr;
		method_name = L"PrintInts";
		delegate_type = L"MyNamespace.PrintIntsDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&printIntsFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(printIntsFunc != nullptr, "printIntsFunc is null");

		printIntsFunc(12345, 67890);

		// --- Test string delegate invocation ---
		typedef void(*print_string)(const char*);
		print_string printStringFunc = nullptr;
		method_name = L"PrintString";
		delegate_type = L"MyNamespace.PrintStringDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&printStringFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(printStringFunc != nullptr, "printStringFunc is null");

		printStringFunc("Hello from C++!");

		// --- Test constructor/destructor delegate invocation ---
		typedef void* (*create_fn)();
		create_fn createFunc = nullptr;

		method_name = L"CreateInstance";
		delegate_type = L"MyNamespace.CreateInstanceDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&createFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(createFunc != nullptr, "createFunc is null");
		
		void* instance = createFunc();

		//////////////////////////////////////////////////////////////////////////

		typedef void* (*create_int_fn)(int);
		create_int_fn createIntFunc = nullptr;

		method_name = L"CreateIntInstance";
		delegate_type = L"MyNamespace.CreateIntInstanceDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&createIntFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(createIntFunc != nullptr, "createIntFunc is null");

		instance = createIntFunc(1753);

		/////////////////////////////////////////////////////////////////////////

		typedef void  (*destroy_fn)(void*);
		destroy_fn destroyFunc = nullptr;

		method_name = L"DestroyInstance";
		delegate_type = L"MyNamespace.DestroyInstanceDelegate, Timefall-ScriptCore";

		rc = loader(
			assembly_path,
			type_name,
			method_name,
			delegate_type,
			nullptr,
			(void**)&destroyFunc
		);

		TF_ASSERT(rc == 0, "load_assembly_and_get_function_pointer failed");
		TF_ASSERT(destroyFunc != nullptr, "destroyFunc is null");

		destroyFunc(instance);
	}

	void ScriptEngine::Shutdown()
	{
		ShutdownHostFxr();
		delete s_ScriptEngineData;
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
}