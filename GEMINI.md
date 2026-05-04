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

### 1. Setup Dependencies
Before building, you must install the required dependencies using the provided vcpkg setup script:
```cmd
.\SetupVcpkg.bat
```
This script bootstraps vcpkg (if not installed) and installs dependencies (`luajit`, `sol2`) specified in `vcpkg.json` for the `x64-windows` triplet.

### 2. Generate Project Files
(Optional/If needed) Run the generation script to prepare the Visual Studio solution:
```cmd
.\GenerateProjectFiles.bat
```

### 3. Build Options
You can open `KraftonEngine.sln` in Visual Studio to build and run the specific target (GameClient, Editor, etc.).

Alternatively, you can use the provided batch scripts for automated builds:
* **Demo Build:**
  ```cmd
  .\DemoBuild.bat
  ```
  This script builds the `Demo` configuration for `x64` using `msbuild`, packages the necessary executable, shaders, assets, and data, and outputs everything to a clean `DemoBuild/` directory at the project root.
* **Release Build:**
  ```cmd
  .\ReleaseBuild.bat
  ```
* **Release with ObjViewer Build:**
  ```cmd
  .\ReleaseWithObjViewerBuild.bat
  ```

## Development Conventions
* **Asset Pipeline:** Raw assets placed in `Data/` are likely parsed and optimized into `.bin` or custom formats stored in `Asset/` at runtime or build time.
* **Scripting:** Gameplay logic for specific actors is often implemented in Lua (e.g., `PIE_AStaticMeshActor_*.lua`).
* **UI:** The engine heavily uses ImGui for the Editor interface and potentially debug overlays in the game client.


# CLAUDE.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
