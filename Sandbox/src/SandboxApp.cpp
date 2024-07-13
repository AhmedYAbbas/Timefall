#include <Timefall.h>
#include <Timefall/Core/EntryPoint.h>

#include "Sandbox2D.h"
#include "ExampleLayer.h"

class Sandbox : public Timefall::Application
{
public:
	Sandbox()
	{
		PushLayer(new Sandbox2D());
	}

	~Sandbox() = default;
};

Timefall::Application* Timefall::CreateApplication()
{
	return new Sandbox();
}
