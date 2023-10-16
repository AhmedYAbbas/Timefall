#pragma once

namespace Timefall
{
	class Timestep
	{
	public:
		Timestep(float time = 0.0f)
			: m_Timestep(time)
		{
		}

		operator float() const { return m_Timestep; }

		float GetSeconds() const { return m_Timestep; }
		float GetMilliseconds() const { return m_Timestep * 1000.f; }

	private:
		float m_Timestep;
	};
}