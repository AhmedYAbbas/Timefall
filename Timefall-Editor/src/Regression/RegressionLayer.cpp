#include "RegressionLayer.h"

#include "Timefall/Core/Application.h"
#include "Timefall/Project/Project.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Timefall/Asset/EditorAssetManager.h"

#include <filesystem>
#include <format>

namespace Timefall
{
	RegressionLayer::RegressionLayer(bool updateGoldens)
		: Layer("RegressionLayer"),
		  m_UpdateGoldens(updateGoldens)
	{}

	void RegressionLayer::Fail(int exitCode, const std::string& reason)
	{
		TF_CORE_ERROR("Regression: {0}", reason);
		Application::Get().Shutdown(exitCode);
	}

	void RegressionLayer::OnAttach()
	{
		auto args = Application::Get().GetSpecification().CommandLineArgs;
		if (args.Count < 2)
		{
			Fail(2, "no project path given (usage: Timefall-Editor.exe <project.tfproj> --regression");
			return;
		}

		std::filesystem::path projectPath = args[1];
		if (!std::filesystem::exists(projectPath) || !Project::Load(projectPath))
		{
			Fail(2, std::format("failed to load project '{0}'", projectPath.string()));
			return;
		}

		ScriptEngine::Init();
		Renderer3D::RegisterBuiltInMeshes(*Project::GetActive()->GetEditorAssetManager());

		TF_CORE_INFO("Regression: project '{0}' loaded, updateGoldens={1}", projectPath.string(), m_UpdateGoldens);

		Application::Get().Shutdown(0);
	}

	void RegressionLayer::OnUpdate(Timestep ts) {}
}
