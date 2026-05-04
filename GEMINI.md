# KraftonEngine - Project Context

## Project Overview

**KraftonEngine** is a custom C++ game engine project. 
- **Architecture**: The engine is broken down into various modules (e.g., `Core`, `Render`, `Scripting`, `UI`, `Sound`, `Physics/Collision`) located under `KraftonEngine/Source/Engine`. The project also contains an `Editor`, `GameClient`, and `ObjViewer`.
- **Graphics/Renderer**: It uses its own DirectX-based renderer (evidenced by the use of `directxtk_desktop_win10` NuGet package and HLSL shaders) rather than relying on external graphics libraries.
- **Dependencies**: 
  - **vcpkg** is used to manage C++ libraries: `luajit`, `sol2`, and `rmlui` (UI framework).
  - **SFML** is used for `audio`, `window`, and `system` management (no graphics/network modules are linked).
  - **ImGui** and **SimpleJSON** are included as third-party sources.
- **Project Structure**: Visual Studio project files (`.sln`, `.vcxproj`, `.vcxproj.filters`) are **auto-generated** from the directory structure using a custom Python script.

## Building and Running

The project relies on a set of batch scripts located at the repository root to streamline setup and builds.

### 1. Setup Dependencies
Before building, ensure dependencies are installed via vcpkg:
```cmd
SetupVcpkg.bat
```
This script will bootstrap a local `vcpkg` installation if necessary and install the manifest dependencies (`vcpkg.json`).

### 2. Generate Project Files
Whenever you add, remove, or move source files, headers, or shaders, you must regenerate the Visual Studio solution and project files:
```cmd
GenerateProjectFiles.bat
```
This runs `Scripts/GenerateProjectFiles.py` which scans the `Source`, `ThirdParty`, and `Shaders` directories to build the MSBuild files.

### 3. Build the Project
You can build the project by opening `KraftonEngine.sln` in Visual Studio 2022 (v143 toolset, Windows 10 SDK) or by using one of the provided batch scripts which use MSBuild:
- `DemoBuild.bat`: Builds the `Demo` x64 configuration and copies the output to a clean `DemoBuild` folder.
- `ReleaseBuild.bat`: Builds the `Release` configuration.
- `ReleaseWithObjViewerBuild.bat`: Builds the Release configuration alongside the ObjViewer.

## Development Conventions

- **Adding New Files**: If you create a new `.cpp`, `.h`, or `.hlsl` file, it will not be automatically detected by MSBuild until you run `GenerateProjectFiles.bat`.
- **Code Organization**:
  - Engine systems are strictly categorized into subdirectories under `KraftonEngine/Source/Engine`.
  - Game-specific logic resides in `KraftonEngine/Source/GameClient`.
  - Editor-specific functionality resides in `KraftonEngine/Source/Editor`.
- **Scripting**: Lua is integrated via `sol2` and `luajit`. Lua scripts are located in `KraftonEngine/LuaScripts`.
- **UI**: RmlUi is used for rich user interfaces, with assets typically structured as `.rcss` and `.rml` in `KraftonEngine/Asset/UI`.
- **Platform**: The project is strictly 64-bit (`x64`). Win32 configurations are present in scripts but actively ignored for dependencies like SFML.
