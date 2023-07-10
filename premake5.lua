workspace "Timefall"
	architecture "x64"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Timefall"
	location "Timefall"
	kind "SharedLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp"
	}
	
	includedirs
	{
		"%{prj.name}/src",
		"%{prj.name}/vendor/spdlog/include"
	}
	
	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"TF_PLATFORM_WINDOWS",
			"TF_BUILD_DLL"				
		}

		postbuildcommands
		{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
		}

	filter "configurations:Debug"
		defines "TF_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "TF_RELEASE"
		optimize "On"

	filter "configurations:Dist"
		defines "TF_DIST"
		optimize "On"

project "Sandbox"
	location "Sandbox"
	kind "ConsoleApp"
	language "C++"

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
		staticruntime "On"
		systemversion "latest"

		defines
		{
			"TF_PLATFORM_WINDOWS"			
		}

	filter "configurations:Debug"
		defines "TF_DEBUG"
		symbols "On"

	filter "configurations:Release"
		defines "TF_RELEASE"
		optimize "On"

	filter "configurations:Dist"
		defines "TF_DIST"
		optimize "On"