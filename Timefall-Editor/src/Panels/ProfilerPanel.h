#pragma once

namespace Timefall
{
	// Glanceable perf HUD: frame-time graph, per-pass CPU ms, renderer + memory stats.
	// Deep analysis (timelines, call stacks) is Tracy's job, not this panel's.
	class ProfilerPanel
	{
	public:
		ProfilerPanel() = default;

		void OnImGuiRender();
	};
}
