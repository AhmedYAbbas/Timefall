#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Debug/Profiler.h"

namespace Timefall
{
	// API-agnostic GPU zone interface; implemented by the active backend (OpenGLGPUProfiler).
	// Zones appear on Tracy's GPU timeline row, time-aligned with CPU zones.
	class TF_API GPUProfiler
	{
	public:
		static void BeginZone(const char* name);
		static void EndZone();

		struct Scope
		{
			Scope(const char* name) { BeginZone(name); }
			~Scope() { EndZone(); }
		};
	};
}

#ifdef TRACY_ENABLE
#define TF_PROFILE_GPU_SCOPE(name) ::Timefall::GPUProfiler::Scope TF_CONCAT(gpuZone, __LINE__)(name)
#else
#define TF_PROFILE_GPU_SCOPE(name)
#endif
