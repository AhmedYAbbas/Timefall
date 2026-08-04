# Timefall

Timefall is a work-in-progress 2D/3D game engine with an editor and C# scripting, originally inspired by [The Cherno's Game Engine Series](https://www.youtube.com/playlist?list=PLlrATfBNZ98dC-V-N3m0Go4deliWHPFwT) and now developed well beyond it. The goal is a Windows-first engine with AAA-grade rendering quality and performance, an editor workflow comparable to commercial tools, and a scripted gameplay layer in C#.

## Current Status

Timefall has grown from a bare-bones 2D renderer into a full forward 3D PBR pipeline with shadows, IBL, an entity-component scene system, C# scripting, 2D physics, an asset pipeline, and an in-editor profiler.

### Renderer

- **Forward 3D PBR renderer** (`Renderer3D`) with metallic-roughness Cook-Torrance direct lighting (GGX/Smith/Schlick), energy compensation, geometric specular anti-aliasing, normal mapping, and an emissive term.
- **Image-based lighting**: HDR equirectangular environment maps baked to cubemaps, GGX-prefiltered specular + diffuse irradiance convolution, and a skybox pass, driven by a `SkyLightComponent`.
- **Shadows** for all three light types: cascaded shadow maps for the directional sun (up to 4 cascades, configurable resolution/split/blend), spot light shadows, and point light (cubemap) shadows — each with both hard and PCSS soft-shadow modes, plus a demand-sized shadow atlas.
- **HDR pipeline**: RGBA16F framebuffer with a tonemap/exposure resolve pass (Reinhard, ReinhardExtended, Hable, ACES Narkowicz/Hill, AgX, Khronos PBR Neutral).
- **Transparency**: alpha-mode blending with a sorted transparent pass, and physically-based inverse-square light falloff.
- **2D batch renderer** (`Renderer2D`) for quads, circles, lines, and MSDF text rendering, alongside the 3D path.

### Scene & ECS

An entt-based ECS with a scene graph (parent/child transform hierarchy). Native components include transforms, sprite/circle renderers, MSDF text, meshes with materials, lights (directional/point/spot), sky lights, cameras, 2D physics bodies and colliders, and script components — plus user-defined components authored in C#.

### Scripting

C# gameplay scripting (`Timefall-ScriptCore`, .NET 9) hosted in-process via `hostfxr`, with assembly hot-reload. The scripting API covers entity transforms and hierarchy, component add/remove/query, entity creation/destruction, input, scene loading, and 2D physics (impulses, velocity, body type), plus a small in-game UI toolkit.

### Physics

2D physics via Box2D — rigidbodies (static/dynamic/kinematic) and box/circle colliders, exposed to both the editor and C# scripts. 3D physics is not yet implemented.

### Assets & Projects

An asset manager (editor and runtime variants) with per-type importers for textures, materials (`.tfmat`), meshes (Assimp-backed, including FBX), and scenes. Projects are `.tfproj` files; scenes serialize to YAML.

### Editor

A full editor built on Dear ImGui: scene hierarchy and inspector, content browser, viewport with gizmos and entity picking, Edit/Play/Simulate scene states with a toolbar, shadow and post-process settings panels, and a real-time profiler panel showing frame/draw-call/triangle stats.

### Profiling

Tracy-based CPU and GPU profiling (`TF_PROFILE_*` macros wrapping a first-party Tracy fork), with RAM and per-category VRAM tracking, integrated into the editor's profiler panel alongside `Renderer3D` frame statistics.

## Getting Started

Visual Studio is recommended; Timefall targets Windows only while the engine matures.

**1. Clone the repository:**

```
git clone --recursive https://github.com/AhmedYAbbas/Timefall
```

If already cloned non-recursively, run `git submodule update --init` to pull in the submodules.

**2. Generate project files:**

Run [`scripts/Win-GenProjects.bat`](scripts/Win-GenProjects.bat) to generate the Visual Studio solution (`Timefall.slnx`) via premake. Re-run it any time premake scripts change or files are added/removed.

**3. Build:**

Open `Timefall.slnx` and build the `Timefall-Editor` startup project (Debug/Release/Dist configurations).
