#pragma once

#include <filesystem>
#include <string>

namespace Timefall
{
	class ScriptEngine
	{
	public:
		static void Init();
		static void Shutdown();

		static void LoadAssembly(const std::filesystem::path& runtimeConfigPath);

	private:
		static bool LoadHostFxr();
		static void ShutdownHostFxr();
	};

	class ScriptClass
	{
	public:
		ScriptClass() = default;

		ScriptClass(const std::filesystem::path& assemblyPath, const std::wstring& typeName)
			: m_AssemblyPath(assemblyPath.wstring()), m_TypeName(typeName)
		{
		}

		~ScriptClass() = default;

		// Typed getter (template must be in header)
		template<typename T>
		T GetMethod(const wchar_t* methodName, const wchar_t* delegateType = nullptr) const
		{
			void* fn = nullptr;
			if (!TryGetFunctionPointer(methodName, delegateType, &fn))
				return reinterpret_cast<T>(nullptr);

			return reinterpret_cast<T>(fn);
		}

		// Invoke helper that casts and calls the function pointer.
		// Usage:
		//   myClass.Invoke<void(*)(int)>(L"PrintInt", L"MyNamespace.PrintIntDelegate, Timefall-ScriptCore", 123);
		template<typename T, typename... Args>
		auto InvokeMethod(const wchar_t* methodName, const wchar_t* delegateType, Args&&... args) const
			-> std::invoke_result_t<T, Args...>
		{
			T fn = GetMethod<T>(methodName, delegateType);
			TF_ASSERT(fn != nullptr, "Managed method not found or invalid delegate type");
			return fn(std::forward<Args>(args)...);
		}

		// Accessors (if needed)
		inline const std::wstring& GetAssemblyPath() const { return m_AssemblyPath; }
		inline const std::wstring& GetTypeName() const { return m_TypeName; }
		
	private:
		bool TryGetFunctionPointer(const wchar_t* methodName, const wchar_t* delegateType, void** out) const;

	private:
		std::wstring m_AssemblyPath;
		std::wstring m_TypeName;
	};
}