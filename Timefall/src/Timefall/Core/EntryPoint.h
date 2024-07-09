#pragma once

#include "Timefall/Core/Core.h"

#ifdef TF_PLATFORM_WINDOWS

int main (int argc, char** argv)
{
	Timefall::Log::Init();

	TF_PROFILE_BEGIN_SESSION("Startup", "TimefallProfile-Startup.json");
	const auto app = Timefall::CreateApplication();
	TF_PROFILE_END_SESSION();

	TF_PROFILE_BEGIN_SESSION("Runtime", "TimefallProfile-Runtime.json");
	app->Run();
	TF_PROFILE_END_SESSION();

	TF_PROFILE_BEGIN_SESSION("Shutdown", "TimefallProfile-Shutdown.json");
	delete app;
	TF_PROFILE_END_SESSION();
}

#endif