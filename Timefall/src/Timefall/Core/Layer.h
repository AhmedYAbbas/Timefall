#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Events/Event.h"

#include "Timefall/Events/KeyEvent.h"
#include "Timefall/Events/MouseEvent.h"
#include "Timefall/Events/ApplicationEvent.h"

#include "Timefall/Core/Timestep.h"

namespace Timefall
{
	class Layer
	{
	public:
		Layer(const std::string& debugName = "Layer");
		virtual ~Layer();

		virtual void OnAttach() {}
		virtual void OnDetach() {}
		virtual void OnUpdate(Timestep ts) {}
		virtual void OnEvent(Event& e) {}
		virtual void OnImGuiRender() {}

		const std::string& GetName() const { return m_DebugName; }

	protected:
		std::string m_DebugName;
	};
}

