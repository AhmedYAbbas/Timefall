project "Sandbox"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"
	staticruntime "on"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	includedirs
	{
		"../Timefall/vendor/spdlog/include",
		"../Timefall/src",
		"../Timefall/vendor",
		"../Timefall/%{IncludeDir.glm}",
		"../Timefall/%{IncludeDir.ImGui}",
		"../Timefall/%{IncludeDir.entt}",
		"../Timefall/%{IncludeDir.box2d}"
	}

	defines
	{
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
	}

	links
	{
		"Timefall",
		"ImGui"
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
