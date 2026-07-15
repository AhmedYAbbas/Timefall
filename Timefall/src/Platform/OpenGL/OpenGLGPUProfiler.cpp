#include "tfpch.h"

#include "Timefall/Renderer/GPUProfiler.h"

#ifdef TRACY_ENABLE
#include <glad/glad.h>
#include <tracy/TracyOpenGL.hpp>

namespace Timefall
{
	namespace
	{
		// Tracy keeps SourceLocationData pointers for the capture's lifetime — storage must be stable.
		const tracy::SourceLocationData* GetSourceLocation(const char* name)
		{
			static std::unordered_map<const char*, tracy::SourceLocationData> s_Locations;

			if (!s_Locations.contains(name))
				s_Locations[name] = tracy::SourceLocationData{name, "GPU", "", 0, 0};

			return &s_Locations[name];
		}

		std::vector<Scope<tracy::GpuCtxScope>> s_ZoneStack;
	}

	void GPUProfiler::BeginZone(const char* name)
	{
		s_ZoneStack.push_back(CreateScope<tracy::GpuCtxScope>(GetSourceLocation(name), true));
	}

	void GPUProfiler::EndZone()
	{
		if (!s_ZoneStack.empty())
			s_ZoneStack.pop_back();
	}
}
#else
namespace Timefall
{
	void GPUProfiler::BeginZone(const char*) {}
	void GPUProfiler::EndZone() {}
}
#endif
