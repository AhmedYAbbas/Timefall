#pragma once

#include "Timefall/Core/Layer.h"

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
		// Logs `reason` at error level, sets the process exit code, and stops the app after this frame
		void Fail(int exitCode, const std::string& reason);

	private:
		bool m_UpdateGoldens = false;
	};
}
