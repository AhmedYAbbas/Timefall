#include "tfpch.h"
#include "Platform/Vulkan/VulkanDeletionQueue.h"

namespace Timefall
{
	void VulkanDeletionQueue::Flush(uint64_t completedValue)
	{
		const auto end = std::ranges::find_if(m_Pending, [&](const Pending& p) {
			return p.RetireAt > completedValue;
		});

		for (auto it = m_Pending.begin(); it != end; ++it)
			it->Fn();

		m_Pending.erase(m_Pending.begin(), end);
	}

	void VulkanDeletionQueue::FlushAll()
	{
		Flush(UINT64_MAX);
	}
}
