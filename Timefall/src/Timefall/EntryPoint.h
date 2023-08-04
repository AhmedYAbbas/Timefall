#pragma once

#ifdef TF_PLATFORM_WINDOWS

inline int main (int argc, char** argv)
{
	Timefall::Log::Init();
	TF_CORE_WARN("Initialized Timefall!");

	const auto app = Timefall::CreateApplication();
	app->Run();
	delete app;
}

#endif