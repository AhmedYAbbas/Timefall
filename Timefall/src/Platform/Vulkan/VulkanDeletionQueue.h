#pragma once

#include "Timefall/RHI/RHITypes.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace Timefall
{
	class VulkanDeletionQueue
	{
	public:
		void Push(uint64_t retireAt, std::function<void()>&& fn) { m_Pending.push_back({retireAt, std::move(fn)}); }

		void Flush(uint64_t completedValue);
		void FlushAll();

	private:
		struct Pending
		{
			uint64_t RetireAt;
			std::function<void()> Fn;
		};

		std::vector<Pending> m_Pending;
	};
}
