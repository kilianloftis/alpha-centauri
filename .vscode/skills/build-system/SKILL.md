---
name: WSL Build System Guide
applyTo: ["**"]
---

# WSL Build System Skill

This skill explains how to configure, build, and run the Alpha Centauri project using the WSL environment.

## Key guidance

- Use the WSL terminal for all build tasks.
- Run `cmake` and `cmake --build` from `/home/kilian/smac` or the workspace root inside WSL.
- Avoid using Windows PowerShell, Windows-native CMake generators, or Visual Studio toolchains.
- Ensure the workspace is opened in VS Code using WSL.

## Recommended commands

```bash
# Configure from WSL
cmake -S . -B build

# Build from WSL
cmake --build build --config Debug

# Run from WSL build directory
./build/alpha-centauri
```

## When to apply this skill

- When the user asks to set up or run the build environment.
- When configuring CMake for the project.
- When the user requests build-related debugging or task execution.

## Notes

- If the current build output shows Windows paths or MSVC tooling, re-run the configuration under WSL.
- Keep the build directory inside the WSL workspace path (`/home/kilian/smac/build`).
