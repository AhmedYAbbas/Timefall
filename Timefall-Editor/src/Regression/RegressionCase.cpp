#include "RegressionCase.h"

#include "Timefall/Core/Log.h"

#include <fstream>
#include <sstream>

#include <yaml-cpp/yaml.h>

namespace Timefall
{
	bool LoadRegressionCase(const std::filesystem::path& path, RegressionCase& out)
	{
		std::ifstream stream(path);
		if (!stream)
		{
			TF_CORE_ERROR("Regression case '{0}': cannot open", path.string());
			return false;
		}

		std::stringstream strStream;
		strStream << stream.rdbuf();

		YAML::Node data;
		try
		{
			data = YAML::Load(strStream.str());
		}
		catch (const YAML::ParserException& e)
		{
			TF_CORE_ERROR("Regression case '{0}': parse error: {1}", path.string(), e.what());
			return false;
		}

		if (!data["Scene"])
		{
			TF_CORE_ERROR("Regression case '{0}': missing 'Scene' key", path.string());
			return false;
		}

		out = RegressionCase{};
		out.CaseFile = path;
		out.ScenePath = data["Scene"].as<std::string>();
		out.Width = data["Width"].as<uint32_t>();
		out.Height = data["Height"].as<uint32_t>();
		out.WarmupFrames = data["WarmupFrames"].as<uint32_t>(out.WarmupFrames);
		out.TimingFrames = data["TimingFrames"].as<uint32_t>(out.TimingFrames);
		out.PixelThreshold = data["PixelThreshold"].as<int>(out.PixelThreshold);
		out.MaxDifferingFraction = data["MaxDifferingFraction"].as<double>(out.MaxDifferingFraction);

		if (auto golden = data["Golden"])
		{
			RegressionGolden g;
			g.Device = golden["Device"].as<std::string>("");
			g.Driver = golden["Driver"].as<std::string>("");

			if (auto counters = golden["Counters"])
			{
				g.DrawCalls = counters["DrawCalls"].as<uint32_t>(0);
				g.ShadowDrawCalls = counters["ShadowDrawCalls"].as<uint32_t>(0);
				g.OpaqueMeshes = counters["OpaqueMeshes"].as<uint32_t>(0);
				g.BlendedMeshes = counters["BlendedMeshes"].as<uint32_t>(0);
				g.TriangleCount = counters["TriangleCount"].as<uint32_t>(0);
				g.MaterialBinds = counters["MaterialBinds"].as<uint32_t>(0);
				g.DirectionalLights = counters["DirectionalLights"].as<uint32_t>(0);
				g.PointLights = counters["PointLights"].as<uint32_t>(0);
				g.SpotLights = counters["SpotLights"].as<uint32_t>(0);
				g.ShadowCasters = counters["ShadowCasters"].as<uint32_t>(0);
				g.CascadeCount = counters["CascadeCount"].as<uint32_t>(0);
				g.PointShadowCubes = counters["PointShadowCubes"].as<uint32_t>(0);
				g.DrawCalls2D = counters["Renderer2D_DrawCalls"].as<uint32_t>(0);
				g.QuadCount = counters["Renderer2D_QuadCount"].as<uint32_t>(0);
				g.CircleCount = counters["Renderer2D_CircleCount"].as<uint32_t>(0);
			}

			out.Golden = g;
		}

		return true;
	}

	void SaveRegressionGolden(const RegressionCase& c)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Scene" << YAML::Value << c.ScenePath.generic_string();
		out << YAML::Key << "Width" << YAML::Value << c.Width;
		out << YAML::Key << "Height" << YAML::Value << c.Height;
		out << YAML::Key << "WarmupFrames" << YAML::Value << c.WarmupFrames;
		out << YAML::Key << "TimingFrames" << YAML::Value << c.TimingFrames;
		out << YAML::Key << "PixelThreshold" << YAML::Value << c.PixelThreshold;
		out << YAML::Key << "MaxDifferingFraction" << YAML::Value << c.MaxDifferingFraction;

		if (c.Golden)
		{
			const RegressionGolden& g = *c.Golden;
			out << YAML::Key << "Golden" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Device" << YAML::Value << g.Device;
			out << YAML::Key << "Driver" << YAML::Value << g.Driver;
			out << YAML::Key << "Counters" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "DrawCalls" << YAML::Value << g.DrawCalls;
			out << YAML::Key << "ShadowDrawCalls" << YAML::Value << g.ShadowDrawCalls;
			out << YAML::Key << "OpaqueMeshes" << YAML::Value << g.OpaqueMeshes;
			out << YAML::Key << "BlendedMeshes" << YAML::Value << g.BlendedMeshes;
			out << YAML::Key << "TriangleCount" << YAML::Value << g.TriangleCount;
			out << YAML::Key << "MaterialBinds" << YAML::Value << g.MaterialBinds;
			out << YAML::Key << "DirectionalLights" << YAML::Value << g.DirectionalLights;
			out << YAML::Key << "PointLights" << YAML::Value << g.PointLights;
			out << YAML::Key << "SpotLights" << YAML::Value << g.SpotLights;
			out << YAML::Key << "ShadowCasters" << YAML::Value << g.ShadowCasters;
			out << YAML::Key << "CascadeCount" << YAML::Value << g.CascadeCount;
			out << YAML::Key << "PointShadowCubes" << YAML::Value << g.PointShadowCubes;
			out << YAML::Key << "Renderer2D_DrawCalls" << YAML::Value << g.DrawCalls2D;
			out << YAML::Key << "Renderer2D_QuadCount" << YAML::Value << g.QuadCount;
			out << YAML::Key << "Renderer2D_CircleCount" << YAML::Value << g.CircleCount;
			out << YAML::EndMap;
			out << YAML::EndMap;
		}

		out << YAML::EndMap;

		std::ofstream fout(c.CaseFile);
		fout << out.c_str();
	}
}
