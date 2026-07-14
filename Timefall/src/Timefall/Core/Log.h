#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

namespace Timefall
{
	class TF_API Log
	{
	public:
		static void Init();

		static Ref<spdlog::logger> GetCoreLogger() { return s_CoreLogger; }
		static Ref<spdlog::logger> GetClientLogger() { return s_ClientLogger; }

	private:
		static Ref<spdlog::logger> s_CoreLogger;
		static Ref<spdlog::logger> s_ClientLogger;
	};
}

template <typename OStream, glm::length_t L, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::vec<L, T, Q>& vector)
{
	return os << glm::to_string(vector);
}

template <typename OStream, glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
inline OStream& operator<<(OStream& os, const glm::mat<C, R, T, Q>& matrix)
{
	return os << glm::to_string(matrix);
}

template <typename OStream, typename T, glm::qualifier Q> inline OStream& operator<<(OStream& os, const glm::qua<T, Q>& quaternion)
{
	return os << glm::to_string(quaternion);
}

// Core log macros
#define TF_CORE_TRACE(...) Timefall::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define TF_CORE_INFO(...) Timefall::Log::GetCoreLogger()->info(__VA_ARGS__)
#define TF_CORE_WARN(...) Timefall::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define TF_CORE_ERROR(...) Timefall::Log::GetCoreLogger()->error(__VA_ARGS__)
#define TF_CORE_CRITICAL(...) Timefall::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define TF_TRACE(...) Timefall::Log::GetClientLogger()->trace(__VA_ARGS__)
#define TF_INFO(...) Timefall::Log::GetClientLogger()->info(__VA_ARGS__)
#define TF_WARN(...) Timefall::Log::GetClientLogger()->warn(__VA_ARGS__)
#define TF_ERROR(...) Timefall::Log::GetClientLogger()->error(__VA_ARGS__)
#define TF_CRITICAL(...) Timefall::Log::GetClientLogger()->critical(__VA_ARGS__)
