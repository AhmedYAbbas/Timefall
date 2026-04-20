#include <Timefall.h>
#include <Timefall/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Timefall
{
	class TimefallEditor : public Application
	{
	public:
		TimefallEditor(const ApplicationSpecification& spec)
			: Application(spec)
		{
			PushLayer(new EditorLayer());
		}

		~TimefallEditor() = default;
	};

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Timefall Editor";
		spec.CommandLineArgs = args;

		return new TimefallEditor(spec);
	}
}