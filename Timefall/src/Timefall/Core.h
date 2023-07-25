#pragma once

#ifdef TF_PLATFORM_WINDOWS
	#ifdef TF_BUILD_DLL
		#define TIMEFALL_API __declspec(dllexport)
	#else
		#define TIMEFALL_API __declspec(dllimport)
	#endif
#else
	#error Timefall only supports windows!
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