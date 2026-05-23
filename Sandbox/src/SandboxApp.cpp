#include <Timefall.h>
#include <Timefall/Core/EntryPoint.h>

#include "Sandbox2D.h"

class Sandbox : public Timefall::Application
{
public:
	Sandbox(const Timefall::ApplicationSpecification& spec)
		: Timefall::Application(spec)
	{
		PushLayer(new Sandbox2D());
	}

	~Sandbox() = default;
};

Timefall::Application* Timefall::CreateApplication(ApplicationCommandLineArgs args)
{
	ApplicationSpecification spec;
	spec.Name = "Sandbox";
	spec.WorkingDirectory = "../TimefallEditor";
	spec.CommandLineArgs = args;

	return new Sandbox(spec);
}
