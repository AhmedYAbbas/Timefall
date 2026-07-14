project "Timefall-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp",
		"%{wks.location}/Timefall/%{IncludeDir.filewatch}/FileWatch.h"
		--"%{wks.location}/Timefall/%{IncludeDir.ImGuizmo}/ImGuizmo.h",
		--"%{wks.location}/Timefall/%{IncludeDir.ImGuizmo}/ImGuizmo.cpp"
	}
	
	includedirs
	{
		"%{wks.location}/Timefall/vendor/spdlog/include",
		"%{wks.location}/Timefall/src",
		"%{wks.location}/Timefall/vendor",
		"%{wks.location}/Timefall/%{IncludeDir.glm}",
		"%{wks.location}/Timefall/%{IncludeDir.filewatch}",
		"%{wks.location}/Timefall/%{IncludeDir.ImGui}",
		"%{wks.location}/Timefall/%{IncludeDir.entt}",
		"%{wks.location}/Timefall/%{IncludeDir.ImGuizmo}",
		"%{wks.location}/Timefall/%{IncludeDir.box2d}",
	}

	defines
	{
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
		"IMGUI_API=__declspec(dllimport)"
	}

	links
	{
		"Timefall"
	}
	
	filter "system:windows"
		systemversion "latest"

		-- Copy Timefall.dll to Timefall-Editor bin folder after build
		postbuildcommands
		{
			"{COPYFILE} \"%{wks.location}Timefall/bin/" .. outputdir .. "/Timefall/Timefall.dll\" \"%{cfg.targetdir}\""
		}

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
