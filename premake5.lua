workspace "Timefall"
    architecture "x86_64" -- This applies to native C++ projects only
	startproject "Timefall-Editor"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	flags
	{
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to Timefall folder (Timefall project directory)
IncludeDir = {}
IncludeDir["GLFW"] = "vendor/GLFW/include"
IncludeDir["Glad"] = "vendor/Glad/include"
IncludeDir["ImGui"] = "vendor/imgui"
IncludeDir["glm"] = "vendor/glm"
IncludeDir["stb_image"] = "vendor/stb_image"
IncludeDir["entt"] = "vendor/entt/include"
IncludeDir["yaml_cpp"] = "vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "vendor/ImGuizmo"
IncludeDir["box2d"] = "vendor/box2d/include"
IncludeDir["hostfxr"] = "vendor/hostfxr/include"

-- Library directories relative to Timefall folder (Timefall project directory)
LibraryDir = {}
LibraryDir["hostfxr"] = "vendor/hostfxr/lib/%{cfg.buildcfg}"

group "Dependencies"
	include "Timefall/vendor/GLFW"
	include "Timefall/vendor/Glad"
	include "Timefall/vendor/imgui"
	include "Timefall/vendor/yaml-cpp"
	include "Timefall/vendor/box2d"

group ""

group "Core"
	include "Timefall"
	--include "Timefall-ScriptCore"
group ""

group "Tools"
	include "Timefall-Editor"
group ""

group "Misc"
	include "Sandbox"
group ""