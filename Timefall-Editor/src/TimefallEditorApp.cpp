#include <Timefall.h>
#include <Timefall/Core/EntryPoint.h>

#include "EditorLayer.h"
#include "Regression/RegressionLayer.h"

namespace Timefall
{
	class TimefallEditor : public Application
	{
	public:
		TimefallEditor(const ApplicationSpecification& spec, bool regression, bool updateGoldens)
			: Application(spec)
		{
			if (regression)
				PushLayer(new RegressionLayer(updateGoldens));
			else
				PushLayer(new EditorLayer());
		}

		~TimefallEditor() = default;
	};

	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "Timefall Editor";
		spec.CommandLineArgs = args;

		bool regression = false, updateGoldens = false;

		for (int i = 2; i < args.Count; i++)
		{
			std::string_view arg = args[i];
			if (arg == "--regression")
				regression = true;
			else if (arg == "--update-goldens")
				updateGoldens = true;
		}

		return new TimefallEditor(spec, regression, updateGoldens);
	}
}
