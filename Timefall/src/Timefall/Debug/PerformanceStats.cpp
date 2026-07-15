#include "tfpch.h"

#include "Timefall/Debug/PerformanceStats.h"

#ifdef TF_PLATFORM_WINDOWS
#include <Psapi.h>
#endif

namespace Timefall
{
	static std::array<float, PerformanceStats::FrameHistorySize> s_FrameHistory = {};
	static size_t s_Cursor = 0;
	static size_t s_FramesRecorded = 0;
	static float s_CurrentMs = 0.0f;

	// Passes accumulate into Pending during the frame; OnFrameEnd publishes to Published.
	static std::vector<std::pair<const char*, float>> s_PendingPasses;
	static std::vector<std::pair<const char*, float>> s_PublishedPasses;

	static uint64_t s_WorkingSetBytes = 0;
	static std::chrono::steady_clock::time_point s_LastMemorySample;

	static void SampleWorkingSet()
	{
#ifdef TF_PLATFORM_WINDOWS
		PROCESS_MEMORY_COUNTERS pmc = {};
		if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
			s_WorkingSetBytes = pmc.WorkingSetSize;
#endif
		TF_PROFILE_PLOT("RAM Working Set", (int64_t)s_WorkingSetBytes);
	}

	void PerformanceStats::OnFrameEnd(float frameTimeMs)
	{
		s_CurrentMs = frameTimeMs;
		s_FrameHistory[s_Cursor] = frameTimeMs;
		s_Cursor = (s_Cursor + 1) % FrameHistorySize;
		s_FramesRecorded = std::min(s_FramesRecorded + 1, FrameHistorySize);

		s_PublishedPasses.swap(s_PendingPasses);
		s_PendingPasses.clear();

		auto now = std::chrono::steady_clock::now();
		if (now - s_LastMemorySample > std::chrono::seconds(1))
		{
			s_LastMemorySample = now;
			SampleWorkingSet();
		}
	}

	void PerformanceStats::RecordPassMs(const char* name, float ms)
	{
		s_PendingPasses.emplace_back(name, ms);
	}

	const std::array<float, PerformanceStats::FrameHistorySize>& PerformanceStats::GetFrameHistory()
	{
		return s_FrameHistory;
	}

	size_t PerformanceStats::GetFrameCursor()
	{
		return s_Cursor;
	}

	float PerformanceStats::GetCurrentMs()
	{
		return s_CurrentMs;
	}

	float PerformanceStats::GetAverageMs()
	{
		if (s_FramesRecorded == 0)
			return 0.0f;

		float sum = 0.0f;
		for (size_t i = 0; i < s_FramesRecorded; ++i)
			sum += s_FrameHistory[i];

		return sum / (float)s_FramesRecorded;
	}

	float PerformanceStats::GetWorstMs()
	{
		float worst = 0.0f;
		for (size_t i = 0; i < s_FramesRecorded; ++i)
			worst = std::max(worst, s_FrameHistory[i]);

		return worst;
	}

	const std::vector<std::pair<const char*, float>>& PerformanceStats::GetPassTimings()
	{
		return s_PublishedPasses;
	}

	uint64_t PerformanceStats::GetWorkingSetBytes()
	{
		return s_WorkingSetBytes;
	}
}
