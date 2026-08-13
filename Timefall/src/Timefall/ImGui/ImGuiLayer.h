#pragma once

#include "Timefall/Core/Layer.h"

#include <vulkan/vulkan.hpp>

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
		bool OnWindowResize(WindowResizeEvent& e);

	private:
		bool m_BlockEvents = true;
		float m_Time = 0.0f;

		vk::DescriptorPool m_ImGuiPool;
	};
}
