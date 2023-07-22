#include <Timefall.h>

class ExampleLayer : public Timefall::Layer
{
public:
	ExampleLayer()
		: Layer("Example")
	{
	}

	void OnUpdate() override
	{
		TF_INFO("ExampleLayer::Update");
	}
	void OnEvent(Timefall::Event& event) override
	{
		TF_TRACE("{0}", event);
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
