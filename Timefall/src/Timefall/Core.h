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
