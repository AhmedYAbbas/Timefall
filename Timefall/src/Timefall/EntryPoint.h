#pragma once

#ifdef TF_PLATFORM_WINDOWS

//extern Timefall::Application* Timefall::CreateApplication();

int main (int argc, char** argv)
{
	auto app = Timefall::CreateApplication();
	app->Run();
	delete app;
}

#endif