-- Load the .NET SDK-style .csproj generator
local dotnet = dofile("../../../premake/dotnet.lua")

-- Define the Sandbox C# project
local sandbox = dotnet.project("Sandbox")
sandbox.location = "."
sandbox.targetFramework = "net9.0"
sandbox.kind = "library"
sandbox.allowUnsafeBlocks = false
sandbox.nullable = "enable"
sandbox.implicitUsings = "enable"
sandbox.rootNamespace = "Sandbox"
table.insert(sandbox.projectReferences, "..\\..\\..\\Timefall-ScriptCore\\Timefall-ScriptCore.csproj")
table.insert(sandbox.excludeFiles, "premake5.lua")
table.insert(sandbox.excludeFiles, "Win-GenProjects.bat")
-- Auto-copy outputs to the editor resources folder
sandbox.copyOutputTo = {
    "$(MSBuildThisFileDirectory)../../../Timefall-Editor/Resources/Scripts"
}

workspace "SandboxProject"
    architecture "x86_64"
	startproject "Sandbox"

	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	multiprocessorcompile "On"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group ""
	externalproject "Timefall-ScriptCore"
		location "../../../Timefall-ScriptCore"
		kind "SharedLib"
		language "C#"

	externalproject "Sandbox"
		location "."
		kind "SharedLib"
		language "C#"
