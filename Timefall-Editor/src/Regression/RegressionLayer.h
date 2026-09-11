#pragma once

#include "Timefall/Core/Layer.h"
#include "Timefall/RHI/RenderTarget.h"
#include "Timefall/RHI/GpuReadback.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Timefall/Renderer/Renderer2D.h"

#include "RegressionCase.h"

#include <cstddef>
#include <vector>

namespace Timefall
{
	// --regression: pushed instead of EditorLayer. No panels, no editor camera, no hot reload,
	// no content browser (Project::Load needs none of them)
	class RegressionLayer : public Layer
	{
	public:
		explicit RegressionLayer(bool updateGoldens);

		void OnAttach() override;
		void OnUpdate(Timestep ts) override;

	private:
		enum class Phase
		{
			NextCase,
			Warmup,
			RequestReadback,
			WaitReadback,
			Timing,
			Compare,
			Finished
		};

		// Logs `reason` at error level, sets the process exit code, and stops the app after this frame
		void Fail(int exitCode, const std::string& reason);

		bool RenderCaseFrame();

	private:
		bool m_UpdateGoldens = false;
		std::vector<RegressionCase> m_Cases;
		size_t m_CaseIndex = 0;
		int m_ExitCode = 0;

		Phase m_Phase = Phase::NextCase;
		Ref<Scene> m_CaseScene;
		Ref<RHI::RenderTarget> m_CaseTarget;
		RHI::GpuReadback m_Readback{"RegressionReadback"};

		uint32_t m_FramesInPhase = 0;
		std::vector<std::byte> m_ReadbackBytes;
		Renderer3D::Statistics m_Stats3D;
		Renderer2D::Statistics m_Stats2D;
		std::vector<float> m_TimingMs;
	};
}
