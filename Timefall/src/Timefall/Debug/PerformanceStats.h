#pragma once

#include "Timefall/Core/Core.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <utility>
#include <vector>

namespace Timefall
{
	// Always-on lightweight stats (no Tracy dependency; alive in Dist). Ring buffer of frame
	// times + named per-pass CPU timings + process RAM, read by the editor's ProfilerPanel.
	class TF_API PerformanceStats
	{
	public:
		static constexpr size_t FrameHistorySize = 300;

		static void OnFrameEnd(float frameTimeMs);
		static void RecordPassMs(const char* name, float ms);

		static const std::array<float, FrameHistorySize>& GetFrameHistory();
		static size_t GetFrameCursor();
		static float GetCurrentMs();
		static float GetAverageMs();
		static float GetWorstMs();
		static const std::vector<std::pair<const char*, float>>& GetPassTimings();
		static uint64_t GetWorkingSetBytes();

		struct ScopedPassTimer
		{
			ScopedPassTimer(const char* name)
				: Name(name),
				  Start(std::chrono::steady_clock::now())
			{}

			~ScopedPassTimer()
			{
				auto elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - Start);
				RecordPassMs(Name, elapsed.count());
			}

			const char* Name;
			std::chrono::steady_clock::time_point Start;
		};
	};
}
