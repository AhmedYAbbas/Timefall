#pragma once

#include "Core.h"
#include "spdlog/spdlog.h"

namespace Timefall
{
	class TIMEFALL_API Log
	{
	public:
		static void Init();

		static std::shared_ptr<spdlog::logger> GetCoreLogger() { return s_CoreLogger; }
		static std::shared_ptr<spdlog::logger> GetClientLogger() { return s_ClientLogger; }

	private:
		static std::shared_ptr<spdlog::logger> s_CoreLogger;
		static std::shared_ptr<spdlog::logger> s_ClientLogger;
	};
}

// Core log macros
#define TF_CORE_TRACE(...) Timefall::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define TF_CORE_INFO(...)  Timefall::Log::GetCoreLogger()->info(__VA_ARGS__)
#define TF_CORE_WARN(...)  Timefall::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define TF_CORE_ERROR(...) Timefall::Log::GetCoreLogger()->error(__VA_ARGS__)
#define TF_CORE_CRITICAL(...) Timefall::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros
#define TF_TRACE(...) Timefall::Log::GetClientLogger()->trace(__VA_ARGS__)
#define TF_INFO(...)  Timefall::Log::GetClientLogger()->info(__VA_ARGS__)
#define TF_WARN(...)  Timefall::Log::GetClientLogger()->warn(__VA_ARGS__)
#define TF_ERROR(...) Timefall::Log::GetClientLogger()->error(__VA_ARGS__)
#define TF_CRITICAL(...) Timefall::Log::GetClientLogger()->critical(__VA_ARGS__)

