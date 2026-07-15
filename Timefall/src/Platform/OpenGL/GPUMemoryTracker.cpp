#include "tfpch.h"

#include "Platform/OpenGL/GPUMemoryTracker.h"

#include <mutex>

namespace Timefall
{
	namespace
	{
		struct CategoryLedger
		{
			std::unordered_map<uint32_t, uint64_t> Allocations;
			uint64_t TotalBytes = 0;
		};

		std::mutex s_Mutex;
		CategoryLedger s_Ledgers[(size_t)GPUMemCategory::Count];

		constexpr const char* s_PlotNames[] = {"VRAM Textures", "VRAM Buffers", "VRAM Framebuffers"};

		void EmitPlots()
		{
			uint64_t total = 0;
			for (size_t i = 0; i < (size_t)GPUMemCategory::Count; ++i)
			{
				TF_PROFILE_PLOT(s_PlotNames[i], (int64_t)s_Ledgers[i].TotalBytes);
				total += s_Ledgers[i].TotalBytes;
			}
			TF_PROFILE_PLOT("VRAM Total", (int64_t)total);
		}
	}

	void GPUMemoryTracker::Track(GPUMemCategory category, uint32_t id, uint64_t bytes)
	{
		std::scoped_lock lock(s_Mutex);
		CategoryLedger& ledger = s_Ledgers[(size_t)category];
		auto [it, inserted] = ledger.Allocations.try_emplace(id, 0);
		ledger.TotalBytes += bytes - it->second;
		it->second = bytes;
		EmitPlots();
	}

	void GPUMemoryTracker::Untrack(GPUMemCategory category, uint32_t id)
	{
		std::scoped_lock lock(s_Mutex);
		CategoryLedger& ledger = s_Ledgers[(size_t)category];
		if (auto it = ledger.Allocations.find(id); it != ledger.Allocations.end())
		{
			ledger.TotalBytes -= it->second;
			ledger.Allocations.erase(it);
			EmitPlots();
		}
	}

	uint64_t GPUMemoryTracker::GetBytes(GPUMemCategory category)
	{
		std::scoped_lock lock(s_Mutex);
		return s_Ledgers[(size_t)category].TotalBytes;
	}

	uint64_t GPUMemoryTracker::GetTotalBytes()
	{
		std::scoped_lock lock(s_Mutex);
		uint64_t total = 0;
		for (const CategoryLedger& ledger : s_Ledgers)
			total += ledger.TotalBytes;
		return total;
	}
}
