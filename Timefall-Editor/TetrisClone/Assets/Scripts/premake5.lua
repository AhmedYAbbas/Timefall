local timefallRootDir = os.getenv("TIMEFALL_DIR")

-- Load the .NET SDK-style .csproj generator
local dotnet = dofile(timefallRootDir .. "/premake/dotnet.lua")

-- Define the TetrisClone C# project
local tetrisclone = dotnet.project("TetrisClone")
tetrisclone.location = "."
tetrisclone.targetFramework = "net9.0"
tetrisclone.kind = "library"
tetrisclone.allowUnsafeBlocks = false
tetrisclone.nullable = "enable"
tetrisclone.implicitUsings = "enable"
tetrisclone.rootNamespace = "TetrisClone"
table.insert(tetrisclone.projectReferences, timefallRootDir .. "/Timefall-ScriptCore/Timefall-ScriptCore.csproj")
table.insert(tetrisclone.excludeFiles, "premake5.lua")
table.insert(tetrisclone.excludeFiles, "Win-GenProjects.bat")
-- Auto-copy outputs to the editor resources folder
tetrisclone.copyOutputTo = {
    timefallRootDir .. "/Timefall-Editor/Resources/Scripts"
}

workspace "TetrisClone"
    architecture "x86_64"
	startproject "TetrisClone"

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
		location(timefallRootDir .. "/Timefall-ScriptCore")
		kind "SharedLib"
		language "C#"

	externalproject "TetrisClone"
		location "."
		kind "SharedLib"
		language "C#"
