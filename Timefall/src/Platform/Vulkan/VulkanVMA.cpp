#include "Timefall/Core/Log.h"

#include <cstdarg>
#include <cstdio>

namespace
{
	// VMA logs printf-style; funnelled into the engine log so an unfreed allocation names itself
	// (see vmaSetAllocationName at every create site) instead of only tripping the leak assert.
	void VmaLeakLog(const char* format, ...)
	{
		char message[512];

		va_list args;
		va_start(args, format);
		std::vsnprintf(message, sizeof(message), format, args);
		va_end(args);

		TF_CORE_ERROR("[VMA] {0}", message);
	}
}

#define VMA_LEAK_LOG_FORMAT(format, ...) ::VmaLeakLog(format, __VA_ARGS__)
#define VMA_IMPLEMENTATION

#include <vk_mem_alloc.h>
