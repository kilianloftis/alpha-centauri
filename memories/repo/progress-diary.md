# Project Status and Architecture Snapshot

## Current State

- The repository currently implements an engine loop plus backend abstractions for graphics and input.
- The application starts in `main.cpp`, initializes `ac::Engine`, and runs a loop until the user presses Enter.
- There is no implemented gameplay, world simulation, AI, or resource management system.
- The graphics backend is pluggable and supports either SFML or a null fallback.
- The input backend is pluggable and supports console input or SFML input.

## Architecture

- Top-level `ac::Engine` composes the application from abstract subsystem interfaces.
- `include/` contains the public interfaces for engine, graphics, and input.
- `src/` contains concrete implementations and platform-specific backends.
- CMake builds the application and may enable SFML support via `USE_SFML`.

## Dependencies

- `Engine` -> `Graphics` and `Input`
- `Graphics` -> SFML when enabled, otherwise `NullGraphics`
- `Input` -> SFML when enabled, otherwise `NullInput`
- No gameplay, world, or AI components are currently present in the source tree
