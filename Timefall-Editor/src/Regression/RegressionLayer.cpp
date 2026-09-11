#include "RegressionLayer.h"

#include "Timefall/Core/Application.h"
#include "Timefall/Project/Project.h"
#include "Timefall/Scripting/ScriptEngine.h"
#include "Timefall/Renderer/Renderer3D.h"
#include "Timefall/Asset/EditorAssetManager.h"
#include "Timefall/Scene/Scene.h"
#include "Timefall/Scene/SceneSerializer.h"
#include "Timefall/RHI/RenderDevice.h"
#include "Timefall/RHI/CommandList.h"

#include "ImageCompare.h"

#include <algorithm>
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
			Fail(2, "no project path given (usage: Timefall-Editor.exe <project.tfproj> --regression)");
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

		const std::filesystem::path regressionDir = Project::GetProjectDirectory() / "Regression";
		if (!std::filesystem::exists(regressionDir))
		{
			Fail(2, std::format("no Regression directory at '{0}'", regressionDir.string()));
			return;
		}

		for (const auto& entry : std::filesystem::directory_iterator(regressionDir))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".yaml")
				continue;

			RegressionCase c;
			if (!LoadRegressionCase(entry.path(), c))
			{
				Fail(2, std::format("malformed case file '{0}'", entry.path().string()));
				return;
			}
			m_Cases.push_back(std::move(c));
		}

		if (m_Cases.empty())
		{
			Fail(2, std::format("no .yaml case files found in '{0}'", regressionDir.string()));
			return;
		}

		TF_CORE_INFO("Regression: {0} case(s) found in '{1}'", m_Cases.size(), regressionDir.string());
	}

	bool RegressionLayer::RenderCaseFrame()
	{
		Renderer3D::SetTargetRenderTarget(m_CaseTarget);
		Renderer2D::SetTargetRenderTarget(m_CaseTarget, false);
		Renderer2D::ResetStats();
		return m_CaseScene->RenderRuntime();
	}

	void RegressionLayer::OnUpdate(Timestep ts)
	{
		if (m_Phase == Phase::Finished)
			return;

		switch (m_Phase)
		{
			case Phase::NextCase:
			{
				if (m_CaseIndex >= m_Cases.size())
				{
					TF_CORE_INFO("Regression: {0} case(s) complete, exit code {1}", m_Cases.size(), m_ExitCode);
					m_Phase = Phase::Finished;
					Application::Get().Shutdown(m_ExitCode);
					return;
				}

				const RegressionCase& c = m_Cases[m_CaseIndex];
				const std::filesystem::path scenePath = Project::GetAssetFileSystemPath(c.ScenePath);

				m_CaseScene = CreateRef<Scene>();
				SceneSerializer serializer(m_CaseScene);
				if (!serializer.DeserializeText(scenePath))
				{
					Fail(2, std::format("case '{0}': failed to load scene '{1}'", c.Name(), scenePath.string()));
					return;
				}

				m_CaseTarget = RHI::RenderTarget::Create({.Width = c.Width,
					.Height = c.Height,
					.ColorFormat = {RHI::Format::RGBA8Unorm, RHI::Format::R32I},
					.ColorCount = 2,
					// Attachment 1 (entity IDs) needs TransferDst too: the engine copies IDs into it
					// internally during resolve, independent of whether anything reads them back.
					.ColorUsage = {RHI::TextureUsage::TransferSrc, RHI::TextureUsage::TransferDst},
					.DebugName = "RegressionTarget"});

				m_CaseScene->OnViewportResize(c.Width, c.Height);

				m_FramesInPhase = 0;
				m_TimingMs.clear();
				m_Phase = Phase::Warmup;
				break;
			}

			case Phase::Warmup:
			{
				if (!RenderCaseFrame())
				{
					Fail(2, std::format("case '{0}': scene has no primary camera", m_Cases[m_CaseIndex].Name()));
					return;
				}

				if (++m_FramesInPhase >= m_Cases[m_CaseIndex].WarmupFrames)
				{
					m_FramesInPhase = 0;
					m_Phase = Phase::RequestReadback;
				}
				break;
			}

			case Phase::RequestReadback:
			{
				if (!RenderCaseFrame())
				{
					Fail(2, std::format("case '{0}': scene has no primary camera", m_Cases[m_CaseIndex].Name()));
					return;
				}

				RHI::CommandList* cmd = RHI::RenderDevice::Get().GetCurrentCommandList();
				if (!cmd || !m_Readback.Request(*cmd, *m_CaseTarget->GetColor(0), {}))
				{
					Fail(2, std::format("case '{0}': readback request failed", m_Cases[m_CaseIndex].Name()));
					return;
				}

				m_Stats3D = Renderer3D::GetStats();
				m_Stats2D = Renderer2D::GetStats();
				m_FramesInPhase = 0;
				m_Phase = Phase::WaitReadback;
				break;
			}

			case Phase::WaitReadback:
			{
				RenderCaseFrame();

				if (auto result = m_Readback.Poll())
				{
					m_ReadbackBytes.assign(result->Bytes.begin(), result->Bytes.end());
					m_FramesInPhase = 0;
					m_Phase = Phase::Timing;
					break;
				}

				if (++m_FramesInPhase > 30)
				{
					Fail(2, std::format("case '{0}': readback did not arrive within 30 frames", m_Cases[m_CaseIndex].Name()));
					return;
				}
				break;
			}

			case Phase::Timing:
			{
				RenderCaseFrame();
				m_TimingMs.push_back(ts.GetMilliseconds());

				if (++m_FramesInPhase >= m_Cases[m_CaseIndex].TimingFrames)
					m_Phase = Phase::Compare;
				break;
			}

			case Phase::Compare:
			{
				RegressionCase& c = m_Cases[m_CaseIndex];

				std::vector<float> sorted = m_TimingMs;
				std::sort(sorted.begin(), sorted.end());
				const float median = sorted[sorted.size() / 2];
				const float p95 = sorted[(size_t)(sorted.size() * 0.95f)];
				TF_CORE_INFO("Regression '{0}': frame time median {1:.2f} ms, p95 {2:.2f} ms", c.Name(), median, p95);

				const uint32_t width = m_CaseTarget->GetWidth();
				const uint32_t height = m_CaseTarget->GetHeight();
				const RHI::Limits& limits = RHI::RenderDevice::Get().GetLimits();

				if (m_UpdateGoldens)
				{
					WritePng(c.GoldenImagePath(), m_ReadbackBytes, width, height);
					c.Golden = RegressionGolden{.Device = limits.DeviceName,
						.Driver = limits.DriverInfo,
						.DrawCalls = m_Stats3D.DrawCalls,
						.ShadowDrawCalls = m_Stats3D.ShadowDrawCalls,
						.OpaqueMeshes = m_Stats3D.OpaqueMeshes,
						.BlendedMeshes = m_Stats3D.BlendedMeshes,
						.TriangleCount = m_Stats3D.TriangleCount,
						.MaterialBinds = m_Stats3D.MaterialBinds,
						.DirectionalLights = m_Stats3D.DirectionalLights,
						.PointLights = m_Stats3D.PointLights,
						.SpotLights = m_Stats3D.SpotLights,
						.ShadowCasters = m_Stats3D.ShadowCasters,
						.CascadeCount = m_Stats3D.CascadeCount,
						.PointShadowCubes = m_Stats3D.PointShadowCubes,
						.DrawCalls2D = m_Stats2D.DrawCalls,
						.QuadCount = m_Stats2D.QuadCount,
						.CircleCount = m_Stats2D.CircleCount};
					SaveRegressionGolden(c);
					TF_CORE_INFO("Regression '{0}': golden updated", c.Name());
				}
				else
				{
					if (!c.Golden)
					{
						Fail(2, std::format("case '{0}': no golden recorded; run with --update-goldens first", c.Name()));
						return;
					}

					const RegressionGolden& g = *c.Golden;
					bool pass = true;

					if (!std::filesystem::exists(c.GoldenImagePath()))
					{
						TF_CORE_ERROR("Regression '{0}': FAIL, no golden image at '{1}'", c.Name(), c.GoldenImagePath().string());
						pass = false;
					}
					else
					{
						ImageCompareResult img = CompareImageToGolden(
							m_ReadbackBytes, width, height, c.GoldenImagePath(), c.PixelThreshold, c.MaxDifferingFraction);
						if (!img.Passed)
						{
							const std::filesystem::path outDir = Project::GetProjectDirectory() / "Regression" / "Output" / c.Name();
							std::filesystem::create_directories(outDir);
							WritePng(outDir / "actual.png", m_ReadbackBytes, width, height);
							WriteFailureImages(m_ReadbackBytes, width, height, c.GoldenImagePath(), outDir);
							TF_CORE_ERROR("Regression '{0}': FAIL image, {1:.4f} differing fraction, max diff {2}, PSNR {3:.1f} dB",
								c.Name(), img.DifferingFraction, img.MaxDifference, img.PSNR);
							pass = false;
						}
					}

					auto checkCounter = [&](const char* name, uint32_t expected, uint32_t actualValue) {
						if (expected != actualValue)
						{
							TF_CORE_ERROR(
								"Regression '{0}': FAIL counter {1}, expected {2}, actual {3}", c.Name(), name, expected, actualValue);
							pass = false;
						}
					};
					checkCounter("DrawCalls", g.DrawCalls, m_Stats3D.DrawCalls);
					checkCounter("ShadowDrawCalls", g.ShadowDrawCalls, m_Stats3D.ShadowDrawCalls);
					checkCounter("OpaqueMeshes", g.OpaqueMeshes, m_Stats3D.OpaqueMeshes);
					checkCounter("BlendedMeshes", g.BlendedMeshes, m_Stats3D.BlendedMeshes);
					checkCounter("TriangleCount", g.TriangleCount, m_Stats3D.TriangleCount);
					checkCounter("MaterialBinds", g.MaterialBinds, m_Stats3D.MaterialBinds);
					checkCounter("DirectionalLights", g.DirectionalLights, m_Stats3D.DirectionalLights);
					checkCounter("PointLights", g.PointLights, m_Stats3D.PointLights);
					checkCounter("SpotLights", g.SpotLights, m_Stats3D.SpotLights);
					checkCounter("ShadowCasters", g.ShadowCasters, m_Stats3D.ShadowCasters);
					checkCounter("CascadeCount", g.CascadeCount, m_Stats3D.CascadeCount);
					checkCounter("PointShadowCubes", g.PointShadowCubes, m_Stats3D.PointShadowCubes);
					checkCounter("Renderer2D.DrawCalls", g.DrawCalls2D, m_Stats2D.DrawCalls);
					checkCounter("Renderer2D.QuadCount", g.QuadCount, m_Stats2D.QuadCount);
					checkCounter("Renderer2D.CircleCount", g.CircleCount, m_Stats2D.CircleCount);

					if (pass)
						TF_CORE_INFO("Regression '{0}': PASS", c.Name());
					else
						m_ExitCode = 1;

					if (g.Device != limits.DeviceName)
						TF_CORE_WARN("Regression '{0}': golden recorded on '{1}', running on '{2}' — a mismatch here only warns", c.Name(),
							g.Device, limits.DeviceName);
				}

				m_CaseIndex++;
				m_Phase = Phase::NextCase;
				break;
			}

			case Phase::Finished: break;
		}
	}
}
