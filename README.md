# Alpha Centauri Rebuild

A C++ project scaffold for rebuilding a turn-based strategy game inspired by Sid Meier's Alpha Centauri.

## What’s included

- `CMakeLists.txt` for a modern CMake build.
- `src/` application code with a small engine skeleton.
- `include/` for public headers.
- `.vscode/` config for build, debug, and C++ tooling.
- AI tooling guidance for GitHub Copilot and VS Code extensions.

## Getting started

1. Open this folder in VS Code using WSL.
2. Install the recommended extensions from `.vscode/extensions.json`.
3. Run the task `CMake: Configure`.
4. Run the task `CMake: Build`.
5. Launch `Alpha Centauri` from the Run view.

## Recommended tools

- `GitHub Copilot Chat` for game design assistance and code generation.
- `CMake Tools` for configure/build integration.
- `C/C++` extension for IntelliSense and debugging.

## Build script

Use the root helper script to configure and build the project:

- `./bd configure` — configure CMake in `build/`
- `./bd build` — build the configured project
- `./bd all` — configure and build in one step
- `./bd clean` — remove the build directory

## Next development steps

- Add world generation and faction systems.
- Design a turn/simulation loop.
- Create a tile map, units, and AI opponents.
- Use Copilot prompts for engine architecture, pathfinding, and AI behavior.
