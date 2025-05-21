workspace "Timefall"
	architecture "x86_64"
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

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Timefall/vendor/GLFW/include"
IncludeDir["Glad"] = "Timefall/vendor/Glad/include"
IncludeDir["ImGui"] = "Timefall/vendor/imgui"
IncludeDir["glm"] = "Timefall/vendor/glm"
IncludeDir["stb_image"] = "Timefall/vendor/stb_image"
IncludeDir["entt"] = "Timefall/vendor/entt/include"
IncludeDir["yaml_cpp"] = "Timefall/vendor/yaml-cpp/include"
IncludeDir["ImGuizmo"] = "Timefall/vendor/ImGuizmo"
IncludeDir["box2d"] = "Timefall/vendor/box2d/include"

group "Dependencies"
	include "Timefall/vendor/GLFW"
	include "Timefall/vendor/Glad"
	include "Timefall/vendor/imgui"
	include "Timefall/vendor/yaml-cpp"
	include "Timefall/vendor/box2d"

group ""

project "Timefall"
	location "Timefall"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	pchheader "tfpch.h"
	pchsource "Timefall/src/tfpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/vendor/stb_image/**.h",
		"%{prj.name}/vendor/stb_image/**.cpp",
		"%{prj.name}/vendor/glm/glm/**.hpp",
		"%{prj.name}/vendor/glm/glm/**.inl",

		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.h",
		"%{prj.name}/vendor/ImGuizmo/ImGuizmo.cpp",

		"%{prj.name}/vendor/base64/base64.hpp"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{prj.name}/vendor/base64",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.glm}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.box2d}"
	}

	defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"yaml-cpp",
		"box2d",
		"opengl32.lib"
	}

	filter "files:Timefall/vendor/ImGuizmo/**.cpp"
		flags { "NoPCH" }
	
	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "TF_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "TF_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "TF_DIST"
		runtime "Release"
		optimize "on"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"Timefall/vendor/spdlog/include",
		"Timefall/src",
		"Timefall/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.entt}"
	}

	defines
	{
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
	}

	links
	{
		"Timefall"
	}
	
	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "TF_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "TF_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "TF_DIST"
		runtime "Release"
		optimize "on"

project "Timefall-Editor"
	location "Timefall-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"Timefall/vendor/spdlog/include",
		"Timefall/src",
		"Timefall/vendor",
		"%{IncludeDir.glm}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.box2d}",
	}

	defines
	{
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
	}

	links
	{
		"Timefall"
	}
	
	filter "system:windows"
		systemversion "latest"

	filter "configurations:Debug"
		defines "TF_DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "TF_RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "TF_DIST"
		runtime "Release"
		optimize "on"