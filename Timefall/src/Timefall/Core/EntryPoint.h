#pragma once

#include "Timefall/Core/Core.h"

#ifdef TF_PLATFORM_WINDOWS

int main (int argc, char** argv)
{
	Timefall::Log::Init();
	TF_CORE_WARN("Initialized Timefall!");

	const auto app = Timefall::CreateApplication();
	app->Run();
	delete app;
}

#endif