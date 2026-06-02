#pragma once

struct hash;

namespace Timefall
{
	class TF_API UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;
		UUID(UUID&&) = default;

		UUID& operator=(const UUID&) = default;
		UUID& operator=(UUID&&) = default;

		// No explicit operator== / operator!=: comparisons go through this implicit conversion
		// (built-in integer comparison). Defining them alongside the conversion makes mixed
		// comparisons like `uuid != 0` ambiguous (two equally-ranked one-conversion paths).
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