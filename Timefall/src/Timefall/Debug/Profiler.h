#pragma once

#define TF_CONCAT_IMPL(a, b) a##b
#define TF_CONCAT(a, b) TF_CONCAT_IMPL(a, b)

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>

// Memory macros capture call stacks (depth 12); drop the S-suffix if per-alloc cost is too high.
#define TF_PROFILE_FRAME() FrameMark
#define TF_PROFILE_FUNCTION() ZoneScoped
#define TF_PROFILE_SCOPE(name) ZoneScopedN(name)
#define TF_PROFILE_THREAD(name) tracy::SetThreadName(name)
#define TF_PROFILE_PLOT(name, value) TracyPlot(name, value)
#define TF_PROFILE_LOG(text, size) TracyMessage(text, size)
#define TF_PROFILE_ALLOC(ptr, size) TracyAllocS(ptr, size, 12)
#define TF_PROFILE_FREE(ptr) TracyFreeS(ptr, 12)
#else
#define TF_PROFILE_FRAME()
#define TF_PROFILE_FUNCTION()
#define TF_PROFILE_SCOPE(name)
#define TF_PROFILE_THREAD(name)
#define TF_PROFILE_PLOT(name, value)
#define TF_PROFILE_LOG(text, size)
#define TF_PROFILE_ALLOC(ptr, size)
#define TF_PROFILE_FREE(ptr)
#endif
