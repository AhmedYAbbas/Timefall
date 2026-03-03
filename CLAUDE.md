# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

Generate Visual Studio 2022 project files:
```batch
scripts\Win-GenProjects.bat
```
Or directly:
```batch
vendor\premake\premake5.exe vs2022
```

Build configurations: Debug, Release, Dist

Build the C# ScriptCore assembly:
```batch
cd Timefall-ScriptCore
dotnet build
```
The DLLs are copied to `Timefall-Editor/resources/Scripts/`.

## Architecture Overview

Timefall is a 2D/3D game engine built with C++20 and C# scripting. The architecture follows a layered design with platform abstraction.

### Project Structure

- **Timefall** - Core engine static library (C++20)
- **Timefall-Editor** - Editor application that links the engine
- **Timefall-ScriptCore** - C# scripting API (.NET 9.0)
- **Timefall-Native** - Native DLL exposing C++ functions to C# via P/Invoke
- **Sandbox** - Example/test application

### Core Engine (Timefall/src/Timefall/)

- **Core/** - Application, Window, Layer system, Input, Logging (spdlog)
- **Renderer/** - OpenGL-based 2D batch renderer, Framebuffers, Shaders, Textures
- **Scene/** - ECS using entt, Components, Scene serialization (YAML)
- **Events/** - Event system (Application, Mouse, Keyboard, Window events)
- **Scripting/** - .NET hosting via hostfxr for C# script execution

### Platform Layer (Timefall/src/Platform/)

- **Windows/** - GLFW window, Windows input
- **OpenGL/** - OpenGL implementations of renderer abstractions

### C# Scripting System

The engine hosts .NET 9.0 runtime via hostfxr. C# scripts in `Timefall-ScriptCore/Scripts/` inherit from `Entity`. Native C++ functions are exposed through `Timefall-Native` DLL using P/Invoke with custom delegates.

Key classes:
- `ScriptEngine` - Initializes .NET runtime, loads assemblies
- `ScriptClass` - Wraps managed types, provides `GetMethod<T>()` and `InvokeMethod<T>()` for calling C# from C++
- `TypeRegistry` (C#) - Registers entity types at runtime

### Key Patterns

- **Smart Pointers**: `Ref<T>` (shared_ptr) and `Scope<T>` (unique_ptr) wrappers in Core.h
- **Precompiled Header**: `tfpch.h` - include standard headers here
- **Platform Macros**: `TF_PLATFORM_WINDOWS`, `TF_DEBUG`, `TF_RELEASE`, `TF_DIST`
- **Assertions**: `TF_ASSERT()`, `TF_CORE_ASSERT()` with debug break

### Dependencies (Timefall/vendor/)

GLFW, Glad, ImGui, glm, entt, yaml-cpp, box2d, spdlog, ImGuizmo, hostfxr
