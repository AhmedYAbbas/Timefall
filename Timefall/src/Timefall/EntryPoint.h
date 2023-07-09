#pragma once

#ifdef TF_PLATFORM_WINDOWS

//extern Timefall::Application* Timefall::CreateApplication();

int main (int argc, char** argv)
{
	Timefall::Log::Init();
	TF_CORE_WARN("Initialized Timefall!");
	int a = 5;
	TF_INFO("Hello! var = {0}", a);

	auto app = Timefall::CreateApplication();
	app->Run();
	delete app;
}

#endif