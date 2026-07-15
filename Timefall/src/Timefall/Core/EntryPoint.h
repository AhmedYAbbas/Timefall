#pragma once

#include "Timefall/Core/Core.h"

#ifdef TF_PLATFORM_WINDOWS

int main(int argc, char** argv)
{
	Timefall::Log::Init();

	const auto app = Timefall::CreateApplication({argc, argv});
	app->Run();
	delete app;
}

#endif