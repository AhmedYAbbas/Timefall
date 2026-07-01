#include "tfpch.h"
#include "PostProcessSettingsPanel.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Renderer/PostProcessSettings.h"

#include <imgui.h>

namespace Timefall
{
	void PostProcessSettingsPanel::OnImGuiRender(const Ref<Scene>& sceneContext)
	{
		ImGui::Begin("Post Process");

		if (!sceneContext)
		{
			ImGui::TextDisabled("No active scene.");
			ImGui::End();
			return;
		}

		PostProcessSettings& p = sceneContext->GetPostProcessSettings();

		ImGui::TextDisabled("Tone Mapping");
		ImGui::Separator();

		static const char* kOperators[] = {
			"None (Linear)", "Reinhard", "Reinhard Extended", "Hable / Uncharted 2",
			"ACES (Narkowicz)", "ACES (Hill)", "AgX", "Khronos PBR Neutral"
		};
		int op = (int)p.Operator;
		if (ImGui::Combo("Operator", &op, kOperators, IM_ARRAYSIZE(kOperators)))
			p.Operator = (ToneMapOperator)op;

		if (p.Operator == ToneMapOperator::ReinhardExtended)
			ImGui::DragFloat("White Point", &p.ReinhardWhitePoint, 0.05f, 0.1f, 20.0f);

		ImGui::Spacing();
		ImGui::TextDisabled("Exposure");
		ImGui::Separator();

		ImGui::DragFloat("Exposure (EV)", &p.ExposureEV, 0.05f, -8.0f, 8.0f);

		ImGui::End();
	}
}
