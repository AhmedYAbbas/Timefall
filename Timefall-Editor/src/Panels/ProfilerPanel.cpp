#include "ProfilerPanel.h"

#include "Timefall/Debug/PerformanceStats.h"
#include "Timefall/Renderer/Renderer2D.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Platform/OpenGL/GPUMemoryTracker.h"

#include <imgui/imgui.h>

#include <algorithm>

namespace Timefall
{
	static float ToMB(uint64_t bytes)
	{
		return (float)bytes / (1024.0f * 1024.0f);
	}

	void ProfilerPanel::OnImGuiRender()
	{
		ImGui::Begin("Profiler");

		const float currentMs = PerformanceStats::GetCurrentMs();
		const float averageMs = PerformanceStats::GetAverageMs();
		const float worstMs = PerformanceStats::GetWorstMs();
		ImGui::Text("Frame: %.2f ms (%.0f FPS)   avg %.2f ms   worst %.2f ms", currentMs, currentMs > 0.0f ? 1000.0f / currentMs : 0.0f,
			averageMs, worstMs);

		// Ring buffer plotted oldest-first via the cursor offset.
		const auto& history = PerformanceStats::GetFrameHistory();
		float scaleMax = std::max(worstMs * 1.2f, 20.0f);
		ImGui::PlotLines("##FrameTimes", history.data(), (int)history.size(), (int)PerformanceStats::GetFrameCursor(), nullptr, 0.0f,
			scaleMax, ImVec2(-1.0f, 80.0f));

		if (ImGui::CollapsingHeader("CPU Passes", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::BeginTable("##Passes", 2, ImGuiTableFlags_SizingStretchProp))
			{
				for (const auto& [name, ms] : PerformanceStats::GetPassTimings())
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f ms", ms);
				}
				ImGui::EndTable();
			}
		}

		if (ImGui::CollapsingHeader("Renderer3D", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto stats = Renderer3D::GetStats();
			ImGui::Text("Draw Calls: %u  (shadow %u, forward %u)", stats.DrawCalls, stats.ShadowDrawCalls, stats.ForwardDrawCalls());
			ImGui::Text("Meshes: %u opaque / %u blended", stats.OpaqueMeshes, stats.BlendedMeshes);
			ImGui::Text("Triangles: %u", stats.TriangleCount);
			ImGui::Text("Indices: %u", stats.IndexCount);
			ImGui::Text("Material Binds: %u", stats.MaterialBinds);
			ImGui::Separator();
			ImGui::Text("Lights: %u dir / %u point / %u spot", stats.DirectionalLights, stats.PointLights, stats.SpotLights);
			ImGui::Text("Shadow Casters: %u", stats.ShadowCasters);
			ImGui::Text("Cascades: %u   Point Cubes: %u", stats.CascadeCount, stats.PointShadowCubes);
		}

		if (ImGui::CollapsingHeader("Renderer2D", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto stats = Renderer2D::GetStats();
			ImGui::Text("Draw Calls: %d", stats.DrawCalls);
			ImGui::Text("Quads: %d", stats.QuadCount);
			ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
			ImGui::Text("Indices: %d", stats.GetTotalIndexCount());
		}

		if (ImGui::CollapsingHeader("Memory", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("RAM (working set): %.1f MB", ToMB(PerformanceStats::GetWorkingSetBytes()));
			ImGui::Separator();
			ImGui::Text("VRAM tracked: %.1f MB", ToMB(GPUMemoryTracker::GetTotalBytes()));
			ImGui::Text("  Textures: %.1f MB", ToMB(GPUMemoryTracker::GetBytes(GPUMemCategory::Textures)));
			ImGui::Text("  Buffers: %.1f MB", ToMB(GPUMemoryTracker::GetBytes(GPUMemCategory::Buffers)));
			ImGui::Text("  Framebuffers: %.1f MB", ToMB(GPUMemoryTracker::GetBytes(GPUMemCategory::Framebuffers)));
		}

		ImGui::End();
	}
}
