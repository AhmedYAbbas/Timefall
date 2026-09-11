#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace Timefall
{
	// Exact-equality counters snapshotted at the readback frame, plus the device they were recorded on.
	struct RegressionGolden
	{
		std::string Device;
		std::string Driver;

		uint32_t DrawCalls = 0;
		uint32_t ShadowDrawCalls = 0;
		uint32_t OpaqueMeshes = 0;
		uint32_t BlendedMeshes = 0;
		uint32_t TriangleCount = 0;
		uint32_t MaterialBinds = 0;
		uint32_t DirectionalLights = 0;
		uint32_t PointLights = 0;
		uint32_t SpotLights = 0;
		uint32_t ShadowCasters = 0;
		uint32_t CascadeCount = 0;
		uint32_t PointShadowCubes = 0;

		uint32_t DrawCalls2D = 0;
		uint32_t QuadCount = 0;
		uint32_t CircleCount = 0;
	};

	struct RegressionCase
	{
		std::filesystem::path CaseFile; // the .yaml itself; also where SaveRegressionGolden writes
		std::filesystem::path ScenePath; // relative to the project's asset directory
		uint32_t Width = 1920;
		uint32_t Height = 1080;
		uint32_t WarmupFrames = 4;
		uint32_t TimingFrames = 120;
		int PixelThreshold = 2;
		double MaxDifferingFraction = 0.001;
		std::optional<RegressionGolden> Golden;

		std::string Name() const { return CaseFile.stem().string(); }
		std::filesystem::path GoldenImagePath() const { return CaseFile.parent_path() / (Name() + ".png"); }
	};

	// False (and logged) on a missing file, a YAML parse error, or a missing 'Scene' key
	bool LoadRegressionCase(const std::filesystem::path& path, RegressionCase& out);

	// Rewrites 'c.CaseFile' in full (fields + Golden, if set) - not a partial edit
	void SaveRegressionGolden(const RegressionCase& c);
}
