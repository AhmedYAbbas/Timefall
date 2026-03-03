# .NET 9 C# Projects with Premake

This document outlines two approaches for integrating .NET 9 C# projects into a premake-based build system while maintaining full IDE support (IntelliSense, debugging, refactoring).

## The Problem

Premake's built-in C# support generates old-style verbose .csproj files for .NET Framework, not SDK-style projects for .NET Core/.NET 5+/.NET 9. Using `kind "Makefile"` as a workaround creates a "fake" project that Visual Studio doesn't understand as a real C# project.

---

## Option 1: External Project (Simplest)

This approach keeps your existing SDK-style .csproj and just references it in the solution.

### Replace `Timefall-ScriptCore/premake5.lua` with:

```lua
externalproject "Timefall-ScriptCore"
    location "Timefall-ScriptCore"
    kind "SharedLib"
    language "C#"

-- If you need other projects to depend on it and trigger builds:
project "Timefall-ScriptCore-PostBuild"
    kind "Utility"

    dependson { "Timefall-ScriptCore" }

    -- Copy outputs after build
    filter "configurations:Debug"
        postbuildcommands
        {
            '{COPYFILE} "%{wks.location}Timefall-ScriptCore/bin/Debug/net9.0/Timefall-ScriptCore.dll" "%{wks.location}Timefall-Editor/resources/Scripts/"',
            '{COPYFILE} "%{wks.location}Timefall-ScriptCore/bin/Debug/net9.0/Timefall-ScriptCore.runtimeconfig.json" "%{wks.location}Timefall-Editor/resources/Scripts/"'
        }

    filter "configurations:Release or Dist"
        postbuildcommands
        {
            '{COPYFILE} "%{wks.location}Timefall-ScriptCore/bin/Release/net9.0/Timefall-ScriptCore.dll" "%{wks.location}Timefall-Editor/resources/Scripts/"',
            '{COPYFILE} "%{wks.location}Timefall-ScriptCore/bin/Release/net9.0/Timefall-ScriptCore.runtimeconfig.json" "%{wks.location}Timefall-Editor/resources/Scripts/"'
        }
```

### Pros
- Minimal changes
- Your existing .csproj works as-is
- Full VS IntelliSense, debugging, refactoring
- VS builds it natively

### Cons
- Premake doesn't control .csproj contents
- You maintain two things (premake5.lua + .csproj)

---

## Option 4: Hybrid Generation (Full Premake Control)

This approach generates the SDK-style .csproj from premake, giving you full control while still getting native VS support.

### Create `premake/dotnet.lua` (reusable module):

```lua
-- premake/dotnet.lua
-- SDK-style .csproj generator for .NET 5+

local p = premake
local dotnet = {}

dotnet.projects = {}

function dotnet.project(name)
    local prj = {
        name = name,
        location = name,
        targetFramework = "net9.0",
        kind = "library",  -- "library" or "exe"
        nullable = "enable",
        implicitUsings = "enable",
        allowUnsafeBlocks = false,
        files = {},
        references = {},
        projectReferences = {},
        packages = {},
        properties = {},
        copyOutputTo = {},
    }

    table.insert(dotnet.projects, prj)
    return prj
end

function dotnet.generate()
    for _, prj in ipairs(dotnet.projects) do
        dotnet.generateProject(prj)
    end
end

function dotnet.generateProject(prj)
    local csprojPath = path.join(prj.location, prj.name .. ".csproj")

    print("Generating " .. csprojPath)

    local file = io.open(csprojPath, "w")
    if not file then
        error("Could not open " .. csprojPath .. " for writing")
    end

    file:write('<Project Sdk="Microsoft.NET.Sdk">\n')
    file:write('\n')
    file:write('  <PropertyGroup>\n')

    -- Output type
    if prj.kind == "exe" then
        file:write('    <OutputType>Exe</OutputType>\n')
    end

    file:write('    <TargetFramework>' .. prj.targetFramework .. '</TargetFramework>\n')
    file:write('    <ImplicitUsings>' .. prj.implicitUsings .. '</ImplicitUsings>\n')
    file:write('    <Nullable>' .. prj.nullable .. '</Nullable>\n')

    if prj.allowUnsafeBlocks then
        file:write('    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n')
    end

    file:write('    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>\n')

    -- Custom properties
    for key, value in pairs(prj.properties) do
        file:write('    <' .. key .. '>' .. value .. '</' .. key .. '>\n')
    end

    file:write('  </PropertyGroup>\n')

    -- Package references
    if #prj.packages > 0 then
        file:write('\n')
        file:write('  <ItemGroup>\n')
        for _, pkg in ipairs(prj.packages) do
            if pkg.version then
                file:write('    <PackageReference Include="' .. pkg.name .. '" Version="' .. pkg.version .. '" />\n')
            else
                file:write('    <PackageReference Include="' .. pkg.name .. '" />\n')
            end
        end
        file:write('  </ItemGroup>\n')
    end

    -- Project references
    if #prj.projectReferences > 0 then
        file:write('\n')
        file:write('  <ItemGroup>\n')
        for _, ref in ipairs(prj.projectReferences) do
            file:write('    <ProjectReference Include="' .. ref .. '" />\n')
        end
        file:write('  </ItemGroup>\n')
    end

    -- Assembly references
    if #prj.references > 0 then
        file:write('\n')
        file:write('  <ItemGroup>\n')
        for _, ref in ipairs(prj.references) do
            file:write('    <Reference Include="' .. ref .. '" />\n')
        end
        file:write('  </ItemGroup>\n')
    end

    -- Post-build copy events
    if #prj.copyOutputTo > 0 then
        file:write('\n')
        file:write('  <Target Name="CopyToDestinations" AfterTargets="Build">\n')
        for _, dest in ipairs(prj.copyOutputTo) do
            file:write('    <Copy SourceFiles="$(OutputPath)$(AssemblyName).dll" DestinationFolder="' .. dest .. '" />\n')
            file:write('    <Copy SourceFiles="$(OutputPath)$(AssemblyName).runtimeconfig.json" DestinationFolder="' .. dest .. '" ContinueOnError="true" />\n')
        end
        file:write('  </Target>\n')
    end

    file:write('\n')
    file:write('</Project>\n')
    file:close()
end

-- Register with premake to generate before VS projects
p.override(p.action, "call", function(base, name)
    -- Generate .csproj files first
    if #dotnet.projects > 0 then
        dotnet.generate()
    end
    -- Then call original action
    base(name)
end)

return dotnet
```

### Update your root `premake5.lua`:

```lua
-- At the top, load the module
local dotnet = dofile("premake/dotnet.lua")

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

-- ... your existing IncludeDir and LibraryDir tables ...

-- Define the C# project (this generates the .csproj)
local scriptCore = dotnet.project("Timefall-ScriptCore")
scriptCore.location = "Timefall-ScriptCore"
scriptCore.targetFramework = "net9.0"
scriptCore.kind = "library"
scriptCore.allowUnsafeBlocks = true
scriptCore.nullable = "enable"
scriptCore.implicitUsings = "enable"
-- Auto-copy outputs to these locations
scriptCore.copyOutputTo = {
    "$(MSBuildThisFileDirectory)../Timefall-Editor/Resources/Scripts"
}
-- Add NuGet packages if needed:
-- table.insert(scriptCore.packages, { name = "Newtonsoft.Json", version = "13.0.3" })

-- ... rest of your workspace setup ...

group "Dependencies"
    include "Timefall/vendor/GLFW"
    include "Timefall/vendor/Glad"
    include "Timefall/vendor/imgui"
    include "Timefall/vendor/yaml-cpp"
    include "Timefall/vendor/box2d"

group ""

group "Core"
    include "Timefall"
    -- Reference the generated .csproj
    externalproject "Timefall-ScriptCore"
        location "Timefall-ScriptCore"
        kind "SharedLib"
        language "C#"

group ""

group "Tools"
    include "Timefall-Editor"
group ""

group "Misc"
    include "Sandbox"
group ""
```

Then delete or simplify `Timefall-ScriptCore/premake5.lua` (it's no longer needed since `externalproject` is in the root file).

---

## Comparison

| Aspect | Option 1 | Option 4 |
|--------|----------|----------|
| Setup complexity | Low | Medium |
| Premake controls .csproj | No | Yes |
| IDE support | Full | Full |
| Maintainability | Two files to maintain | Single source of truth |
| NuGet packages | Edit .csproj manually | Define in premake |
| Post-build copies | Utility project or manual | Built into .csproj |

---

## Recommendation

**Start with Option 1** to verify the `externalproject` approach works for your workflow. If you find yourself wanting premake to fully control the .csproj contents (e.g., for consistency, or to use premake tokens for paths), then upgrade to Option 4.
