#pragma once

struct hash;

namespace Timefall
{
	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;
		UUID(UUID&&) = default;
		UUID& operator=(const UUID&) = default;
		UUID& operator=(UUID&&) = default;
		bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
		bool operator!=(const UUID& other) const { return !(*this == other); }

		operator uint64_t() const { return m_UUID; }

	private:
		uint64_t m_UUID;
	};
}

namespace std
{
	template<>
	struct hash<Timefall::UUID>
	{
		size_t operator()(const Timefall::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};
}