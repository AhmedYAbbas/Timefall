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
        rootNamespace = nil,
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

    -- BOM for UTF-8 (matches VS default)
    file:write('\xef\xbb\xbf')
    file:write('<Project Sdk="Microsoft.NET.Sdk">\n')
    file:write('\n')

    -- Main PropertyGroup with common settings
    file:write('  <PropertyGroup>\n')

    -- Output type
    if prj.kind == "exe" then
        file:write('    <OutputType>Exe</OutputType>\n')
    end

    file:write('    <TargetFramework>' .. prj.targetFramework .. '</TargetFramework>\n')

    if prj.rootNamespace then
        file:write('    <RootNamespace>' .. prj.rootNamespace .. '</RootNamespace>\n')
    end

    file:write('    <ImplicitUsings>' .. prj.implicitUsings .. '</ImplicitUsings>\n')
    file:write('    <Nullable>' .. prj.nullable .. '</Nullable>\n')

    if prj.allowUnsafeBlocks then
        file:write('    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n')
    end

    file:write('    <GenerateRuntimeConfigurationFiles>true</GenerateRuntimeConfigurationFiles>\n')
    file:write('    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n')
    file:write('    <Configurations>Debug;Release;Dist</Configurations>\n')
    -- Use AnyCPU as the platform for VS compatibility (PlatformTarget controls actual output)
    file:write('    <Platforms>AnyCPU</Platforms>\n')
    file:write('    <PlatformTarget>x64</PlatformTarget>\n')

    -- Custom properties
    for key, value in pairs(prj.properties) do
        file:write('    <' .. key .. '>' .. value .. '</' .. key .. '>\n')
    end

    file:write('  </PropertyGroup>\n')

    -- Debug configuration (AnyCPU platform for VS compatibility)
    file:write('\n')
    file:write('  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Debug|AnyCPU\'">\n')
    file:write('    <Optimize>false</Optimize>\n')
    file:write('    <DebugType>full</DebugType>\n')
    file:write('    <DefineConstants>DEBUG;TRACE</DefineConstants>\n')
    file:write('    <OutputPath>bin\\Debug\\</OutputPath>\n')
    file:write('  </PropertyGroup>\n')

    -- Release configuration (AnyCPU platform for VS compatibility)
    file:write('\n')
    file:write('  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Release|AnyCPU\'">\n')
    file:write('    <Optimize>true</Optimize>\n')
    file:write('    <DebugType>pdbonly</DebugType>\n')
    file:write('    <DefineConstants>RELEASE;TRACE</DefineConstants>\n')
    file:write('    <OutputPath>bin\\Release\\</OutputPath>\n')
    file:write('  </PropertyGroup>\n')

    -- Dist configuration (AnyCPU platform for VS compatibility)
    file:write('\n')
    file:write('  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'Dist|AnyCPU\'">\n')
    file:write('    <Optimize>true</Optimize>\n')
    file:write('    <DebugType>pdbonly</DebugType>\n')
    file:write('    <DefineConstants>DIST;TRACE</DefineConstants>\n')
    file:write('    <OutputPath>bin\\Dist\\</OutputPath>\n')
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

-- Fix slnx configuration mappings for C# projects with x64 platform
-- Premake generates incorrect BuildType entries; we need Configuration entries
function dotnet.fixSlnxConfigurations(workspaceName)
    local slnxPath = workspaceName .. ".slnx"
    local file = io.open(slnxPath, "r")
    if not file then return end

    local content = file:read("*all")
    file:close()

    -- For each dotnet project, fix the configuration mappings
    for _, prj in ipairs(dotnet.projects) do
        local csprojPath = prj.location .. "\\" .. prj.name .. ".csproj"

        -- Escape special pattern characters in the path
        local escapedPath = csprojPath:gsub("([%^%$%(%)%%%.%[%]%*%+%-%?])", "%%%1")

        -- Pattern to match the project element and its contents
        local pattern = '(<Project Path="' .. escapedPath .. '"[^>]*>)(.-)(\n%s*</Project>)'

        content = content:gsub(pattern, function(openTag, inner, closeTag)
            -- Map solution x64 platform to project AnyCPU platform (SDK-style projects use AnyCPU with PlatformTarget for actual output)
            local newInner = '\n      <Configuration Solution="Debug|x64" Project="Debug|AnyCPU" Build="true" />\n      <Configuration Solution="Release|x64" Project="Release|AnyCPU" Build="true" />\n      <Configuration Solution="Dist|x64" Project="Dist|AnyCPU" Build="true" />\n    '
            return openTag .. newInner .. closeTag
        end)
    end

    file = io.open(slnxPath, "w")
    if file then
        file:write(content)
        file:close()
        print("Fixed slnx configuration mappings for C# projects")
    end
end

-- Register with premake to generate before VS projects
p.override(p.action, "call", function(base, name)
    -- Generate .csproj files first
    if #dotnet.projects > 0 then
        dotnet.generate()
    end
    -- Then call original action
    base(name)
    -- Fix slnx configuration mappings for C# projects
    if #dotnet.projects > 0 then
        -- Get workspace name from the first workspace in premake's global state
        local wks = p.global.getWorkspace(1)
        if wks then
            dotnet.fixSlnxConfigurations(wks.name)
        end
    end
end)

return dotnet
