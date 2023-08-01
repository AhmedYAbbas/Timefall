workspace "Timefall"
	architecture "x64"
	startproject "Sandbox"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Timefall/vendor/GLFW/include"
IncludeDir["Glad"] = "Timefall/vendor/Glad/include"
IncludeDir["ImGui"] = "Timefall/vendor/imgui"

group "Dependencies"
	include "Timefall/vendor/GLFW"
	include "Timefall/vendor/Glad"
	include "Timefall/vendor/ImGui"

group ""

project "Timefall"
	location "Timefall"
	kind "SharedLib"
	language "C++"
	staticruntime "Off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	pchheader "tfpch.h"
	pchsource "Timefall/src/tfpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends"
	}

	links
	{
		"GLFW",
		"Glad",
		"ImGui",
		"opengl32.lib"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

		defines
		{
			"TF_PLATFORM_WINDOWS",
			"TF_BUILD_DLL",
			"GLFW_INCLUDE_NONE"
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}

	filter "configurations:Debug"
		defines "TF_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "TF_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "TF_DIST"
		runtime "Release"
		optimize "On"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"
	staticruntime "Off"

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
		"Timefall/src"
	}

	links
	{
		"Timefall"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		systemversion "latest"

		defines
		{
			"TF_PLATFORM_WINDOWS"			
		}

	filter "configurations:Debug"
		defines "TF_DEBUG"
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
		defines "TF_RELEASE"
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
		defines "TF_DIST"
		runtime "Release"
		optimize "On"