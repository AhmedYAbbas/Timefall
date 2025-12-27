project "Timefall-Native"
	kind "SharedLib"
	language "C++"
	cppdialect "C++20"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}
	
	includedirs
	{
		"%{wks.location}/Timefall/vendor/spdlog/include",
		"%{wks.location}/Timefall/src",
		"%{wks.location}/Timefall/vendor",
		"%{wks.location}/Timefall/%{IncludeDir.glm}",
		"%{wks.location}/Timefall/%{IncludeDir.ImGui}",
		"%{wks.location}/Timefall/%{IncludeDir.entt}",
		"%{wks.location}/Timefall/%{IncludeDir.ImGuizmo}",
		"%{wks.location}/Timefall/%{IncludeDir.box2d}",
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