#pragma once

#include "Core.h"
#include "Events/Event.h"

#include "Timefall/Events/KeyEvent.h"
#include "Timefall/Events/MouseEvent.h"
#include "Timefall/Events/ApplicationEvent.h"

namespace Timefall
{
	class TIMEFALL_API Layer
	{
	public:
		Layer(const std::string& debugName = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate() {}
		virtual void OnEvent(Event& event) {}
		virtual void OnImGuiRender() {}

		const std::string& GetName() const { return m_DebugName; }

	protected:
		std::string m_DebugName;
	};
}

