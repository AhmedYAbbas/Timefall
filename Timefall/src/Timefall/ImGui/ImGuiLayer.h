#pragma once

#include "Timefall/Core/Layer.h"

namespace Timefall
{
	class TF_API ImGuiLayer : public Layer
	{
	public:
		ImGuiLayer();
		~ImGuiLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnEvent(Event& e) override;

		void Begin();
		void End();

		void BlockEvents(bool block) { m_BlockEvents = block; }
		void SetDarkThemeColors();

		uint32_t GetActiveWidgetID() const;

	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;
	};
}
