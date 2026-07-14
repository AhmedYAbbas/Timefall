project "Timefall"
	kind "SharedLib"
	language "C++"
	cppdialect "C++23"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("int/" .. outputdir .. "/%{prj.name}")

	pchheader "tfpch.h"
	pchsource "src/tfpch.cpp"

	files
	{
		"src/**.h",
		"src/**.cpp",
		"vendor/stb_image/**.h",
		"vendor/stb_image/**.cpp",
		"vendor/glm/glm/**.hpp",
		"vendor/glm/glm/**.inl",

		"vendor/imgui/imgui.cpp",
		"vendor/imgui/imgui_widgets.cpp",
		"vendor/imgui/imgui_draw.cpp",
		"vendor/imgui/imgui_tables.cpp",
		"vendor/imgui/imgui_demo.cpp",
		
		"vendor/ImGuizmo/ImGuizmo.h",
		"vendor/ImGuizmo/ImGuizmo.cpp",

		"vendor/base64/base64.hpp"
	}
	
	includedirs
	{
		"src",
		"vendor/spdlog/include",
		"vendor/base64",
		"%{IncludeDir.GLFW}",
		"%{IncludeDir.Glad}",
		"%{IncludeDir.filewatch}",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.glm}",
		"%{IncludeDir.msdfgen}",
		"%{IncludeDir.msdf_atlas_gen}",
		"%{IncludeDir.stb_image}",
		"%{IncludeDir.entt}",
		"%{IncludeDir.yaml_cpp}",
		"%{IncludeDir.ImGuizmo}",
		"%{IncludeDir.box2d}",
		"%{IncludeDir.hostfxr}",
		"%{IncludeDir.assimp}"
	}

	defines
	{
		"TF_BUILD_DLL",

		"_CRT_SECURE_NO_WARNINGS",
		"_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING",
		"GLFW_INCLUDE_NONE",
		"YAML_CPP_STATIC_DEFINE",

		"IMGUI_API=__declspec(dllexport)"
	}

	links
	{
		"GLFW",
		"Glad",
		--"ImGui",
		"yaml-cpp",
		"box2d",
		"opengl32.lib",

		"msdf-atlas-gen",

		"assimp",

		"%{LibraryDir.hostfxr}/hostfxr.lib",
		"%{LibraryDir.hostfxr}/hostpolicy.lib",
		"%{LibraryDir.hostfxr}/nethost.lib",

		"%{LibraryDir.hostfxr}/hostfxr.dll",
		"%{LibraryDir.hostfxr}/nethost.dll",

		--Windows--
		"Winmm.lib",
		"Bcrypt.lib",
		"Ws2_32.lib",
	}

	filter "files:vendor/ImGuizmo/**.cpp"
		enablepch "Off"

	filter "files:vendor/imgui/**.cpp"
		enablepch "Off"

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
