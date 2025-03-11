#include <Timefall.h>
#include <Timefall/Core/EntryPoint.h>

#include "EditorLayer.h"

namespace Timefall
{
	class TimefallEditor : public Application
	{
	public:
		TimefallEditor()
			: Application("Timefall Editor")
		{
			PushLayer(new EditorLayer());
		}

		~TimefallEditor() = default;
	};

	Application* CreateApplication()
	{
		return new TimefallEditor();
	}
}