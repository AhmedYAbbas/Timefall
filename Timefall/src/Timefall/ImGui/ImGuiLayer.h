#pragma once

#include "Timefall/Core/Layer.h"

namespace Timefall
{
	class ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;
		
		void Begin();
		void End();

	private:
		float m_Time = 0.0f;
	};
}

