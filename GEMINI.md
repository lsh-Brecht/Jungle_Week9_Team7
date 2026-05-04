# KraftonEngine

## Project Overview
KraftonEngine is a custom 3D game engine and game client written in C++ for Windows. It utilizes DirectX (via DirectX Tool Kit) for rendering and HLSL for shaders. The engine supports scripting through Lua (integrated via `luajit` and `sol2`) and features an Editor built with ImGui.

The codebase is divided into several main components:
* **Engine:** The core runtime architecture of the game engine.
* **Editor:** The graphical engine editor/toolset interface.
* **GameClient:** The actual game implementation built on top of the engine.
* **ObjViewer:** A standalone tool for viewing 3D `.obj` models.

## Key Technologies & Dependencies
* **C++17/20:** Primary programming language.
* **DirectX 11/12:** Rendering API (via `directxtk_desktop_win10`).
* **HLSL:** Shader programming (located in `KraftonEngine/Shaders/`).
* **vcpkg:** C++ package manager used for managing dependencies.
* **Lua:** Scripting language (`luajit`, `sol2` bindings) for game logic and configuration (located in `KraftonEngine/LuaScripts/`).
* **ImGui:** Immediate mode GUI library used for the Editor.

## Directory Structure
* `KraftonEngine/Source/`: Contains the C++ source code for `Editor`, `Engine`, `GameClient`, and `ObjViewer`.
* `KraftonEngine/Shaders/`: HLSL shader files categorized by functionality (Common, Editor, Geometry, Lighting, etc.).
* `KraftonEngine/Asset/`: Compiled/processed game assets such as Scenes, Prefabs, Materials, and Textures.
* `KraftonEngine/Data/`: Raw source data for assets like `.obj`, `.mtl`, and image files.
* `KraftonEngine/LuaScripts/`: Lua scripts for level logic and actor behaviors.
* `KraftonEngine/Bin/`: Compilation output directories for different build configurations.
* `Scripts/`: Helper scripts, including Python scripts for generating project files.

## Building and Running

The project is built using Visual Studio (MSBuild). 
