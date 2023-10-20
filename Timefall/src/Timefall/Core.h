#pragma once

#include <memory>

#ifdef TF_PLATFORM_WINDOWS
#if TF_DYNAMIC_LINK
	#ifdef TF_BUILD_DLL
		#define TIMEFALL_API __declspec(dllexport)
	#else
		#define TIMEFALL_API __declspec(dllimport)
	#endif
#else
	#define TIMEFALL_API
#endif
#else
	#error Timefall only supports windows!
#endif

#if TF_DEBUG
	#define TF_ENABLE_ASSERTS
#endif

#ifdef TF_ENABLE_ASSERTS
	#define TF_ASSERT(x, ...) { if (!(x)) { TF_ERROR("Assertion failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define TF_CORE_ASSERT(x, ...) { if (!(x)) { TF_CORE_ERROR("Assertion failed: {0}", __VA_ARGS__); __debugbreak(); } } 
#else
	#define TF_ASSERT(x, ...) 
	#define TF_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << (x))

#define TF_BIND_EVENT_FN(fn) std::bind(fn, this, std::placeholders::_1)

namespace Timefall
{
	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T>
	using Scope = std::unique_ptr<T>;
}