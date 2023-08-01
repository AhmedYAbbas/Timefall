#include <Timefall.h>

class ExampleLayer : public Timefall::Layer
{
public:
	ExampleLayer()
		: Layer("Example") {}

	void OnUpdate() override
	{
		
	}

	void OnEvent(Timefall::Event& event) override
	{
		if (event.GetEventType() == Timefall::EventType::KeyPressed)
		{
			Timefall::KeyPressedEvent& e = dynamic_cast<Timefall::KeyPressedEvent&>(event);
			if (e.GetKeyCode() == TF_KEY_TAB)
				TF_TRACE("Tab key is pressed");

			TF_TRACE("{0}", (char)e.GetKeyCode());
		}
	}
};

class Sandbox : public Timefall::Application
{
public:
	Sandbox()
	{
		PushLayer(new ExampleLayer());
		PushLayer(new Timefall::ImGuiLayer());
	}

	~Sandbox()
	{

	}
};

Timefall::Application* Timefall::CreateApplication()
{
	return new Sandbox();
}
