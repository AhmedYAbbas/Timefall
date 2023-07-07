#include <Timefall.h>

class Sandbox : public Timefall::Application
{
public:
	Sandbox()
	{
		
	}

	~Sandbox()
	{

	}
};

Timefall::Application* Timefall::CreateApplication()
{
	return new Sandbox();
}
