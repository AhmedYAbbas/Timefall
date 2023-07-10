#include "tfpch.h"

#include "Application.h"
#include "Events/ApplicationEvent.h"
#include "Timefall/Log.h"

namespace Timefall
{
	Application::Application()
	{

	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		WindowResizeEvent e(1920, 1080);
		TF_TRACE(e);

		while (true);
	}
}
