# Timefall
Welcome to the repository for my game engine project! This README will provide an overview of the project, and its current status.

## Overview
 Timefall Enigne is a work in progress inspired by <a href="https://www.youtube.com/playlist?list=PLlrATfBNZ98dC-V-N3m0Go4deliWHPFwT">The Cherno's tutorial series</a>. The goal of this project is to create a powerful and flexible game engine that can be used to develop a wide range of games.

## Current Status
As mentioned earlier, this project is in its early work-in-progress stage. I have successfully implemented some fundamental components, including:

- **Events:** Including MouseEvents, KeyboardEvents, WindowEvents.
- **Logging:** Basic Logging system that prints to the console.
- **Layers:** Layers system to associate actions to only specific layers.
- **Input Handling:** Basic input handling for user interaction with the game.
- **2D Renderer:** 2D Rendering system using OpenGL to draw colored and textured quads on screen.
- **Batch Rendering:** Batch rendering for geometry, transforms, colors, and textures.

Please note that while these components are functional, they may lack advanced features and optimizations.

***

## Getting Started
Visual Studio 2022 is recommended, Timefall is officially untested on other development environments whilst I focus on a Windows build.

<ins>**1. Downloading the repository:**</ins>

Start by cloning the repository with `git clone --recursive https://github.com/AhmedYAbbas/Timefall`.

If the repository was cloned non-recursively previously, use `git submodule update --init` to clone the necessary submodules.

<ins>**2. Configuring the dependencies:**</ins>

Run the [Win-GenProjects.bat](https://github.com/AhmedYAbbas/Timefall/blob/main/scripts/Win-GenProjects.bat) file found in `scripts` folder. Script file will get executed, which will then generate a Visual Studio solution file for user's usage.

If changes are made, or if you want to regenerate project files, rerun the [Win-GenProjects.bat](https://github.com/AhmedYAbbas/Timefall/blob/main/scripts/Win-GenProjects.bat) script file found in `scripts` folder.

***

## The Plan
The plan for Timefall is two-fold: to create a powerful 2D and 3D engine, while also learning game engine design and architecture.

### Main features to come:
- Fast 2D rendering (UI, particles, sprites, etc.)
- High-fidelity Physically-Based 3D rendering (this will be expanded later, 2D to come first)
- Support for Mac, Linux, Android and iOS
    - Native rendering API support (DirectX, Vulkan, Metal)
- Fully featured viewer and editor applications
- Fully scripted interaction and behavior
- Integrated 3rd party 2D and 3D physics engine
- Procedural terrain and world generation
- Artificial Intelligence
- Audio system