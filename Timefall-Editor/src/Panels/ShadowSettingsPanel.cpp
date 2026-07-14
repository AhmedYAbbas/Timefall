#include "tfpch.h"
#include "ShadowSettingsPanel.h"

#include "Timefall/Scene/Scene.h"
#include "Timefall/Renderer/ShadowSettings.h"

#include <imgui.h>

namespace Timefall
{
	void ShadowSettingsPanel::OnImGuiRender(const Ref<Scene>& sceneContext)
	{
		ImGui::Begin("Shadows");

		if (!sceneContext)
		{
			ImGui::TextDisabled("No active scene.");
			ImGui::End();
			return;
		}

		ShadowSettings& s = sceneContext->GetShadowSettings();

		ImGui::TextDisabled("Directional Sun (CSM)");
		ImGui::Separator();

		static const uint32_t kResolutions[] = {512, 1024, 2048, 4096};
		static const char* kResolutionLabels[] = {"512", "1024", "2048", "4096"};
		int resIndex = 2;
		for (int i = 0; i < IM_ARRAYSIZE(kResolutions); ++i)
			if (kResolutions[i] == s.ShadowMapResolution)
				resIndex = i;
		if (ImGui::Combo("Resolution", &resIndex, kResolutionLabels, IM_ARRAYSIZE(kResolutionLabels)))
			s.ShadowMapResolution = kResolutions[resIndex];

		int spotResIndex = 1;
		for (int i = 0; i < IM_ARRAYSIZE(kResolutions); ++i)
			if (kResolutions[i] == s.SpotShadowResolution)
				spotResIndex = i;
		if (ImGui::Combo("Spot Resolution", &spotResIndex, kResolutionLabels, IM_ARRAYSIZE(kResolutionLabels)))
			s.SpotShadowResolution = kResolutions[spotResIndex];

		static const uint32_t kPointResolutions[] = {256, 512, 1024};
		static const char* kPointResLabels[] = {"256", "512", "1024"};
		int pointResIndex = 1;
		for (int i = 0; i < IM_ARRAYSIZE(kPointResolutions); ++i)
			if (kPointResolutions[i] == s.PointShadowResolution)
				pointResIndex = i;
		if (ImGui::Combo("Point Resolution", &pointResIndex, kPointResLabels, IM_ARRAYSIZE(kPointResLabels)))
			s.PointShadowResolution = kPointResolutions[pointResIndex];

		int cascadeCount = (int)s.CascadeCount;
		if (ImGui::SliderInt("Cascade Count", &cascadeCount, 1, (int)ShadowSettings::MaxCascades))
			s.CascadeCount = (uint32_t)cascadeCount;

		ImGui::DragFloat("Max Distance", &s.MaxShadowDistance, 1.0f, 1.0f, 1000.0f);
		ImGui::SliderFloat("Split Lambda", &s.SplitLambda, 0.0f, 1.0f);
		ImGui::SliderFloat("Cascade Blend", &s.CascadeBlend, 0.0f, 0.5f);

		static const char* kCullLabels[] = {"Back", "Front (2nd-depth)", "None"};
		int cullIndex = (int)s.CullMode;
		if (ImGui::Combo("Caster Culling", &cullIndex, kCullLabels, IM_ARRAYSIZE(kCullLabels)))
			s.CullMode = (ShadowCullMode)cullIndex;

		ImGui::Spacing();
		ImGui::TextDisabled("Filtering");
		ImGui::Separator();

		ImGui::Checkbox("Soft Shadows (PCSS)", &s.SoftShadows);

		int blockerSamples = (int)s.BlockerSearchSamples;
		if (ImGui::SliderInt("Blocker Samples", &blockerSamples, 4, 64))
			s.BlockerSearchSamples = (uint32_t)blockerSamples;

		int pcfSamples = (int)s.PCFSamples;
		if (ImGui::SliderInt("PCF Samples", &pcfSamples, 4, 64))
			s.PCFSamples = (uint32_t)pcfSamples;

		ImGui::Spacing();
		ImGui::TextDisabled("Debug");
		ImGui::Separator();

		ImGui::Checkbox("Visualize Cascades", &s.VisualizeCascades);

		ImGui::End();
	}
}
