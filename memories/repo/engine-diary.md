# Engine Component Current State

## Current State

- `ac::Engine` is the central runtime entry point for the application.
- It constructs the graphics and input backends through `CreateGraphics()` and `CreateInput()`.
- `Engine::Run()` initializes the engine, prints a welcome message, then enters a loop until the user presses Enter.
- Each loop iteration clears the display, draws static prompt text and the last captured key, presents the rendered frame, and polls input.
- There is no gameplay simulation, world model, AI, or entity system implemented yet.

## Architecture

- `Engine` owns:
  - `std::unique_ptr<Graphics> m_graphics`
  - `std::unique_ptr<Input> m_input`
- Backend creation is delegated to factory functions:
  - `CreateGraphics()` selects `SFMLGraphics` or `NullGraphics`
  - `CreateInput()` selects `SFMLInput` or `NullInput`
- Private methods encapsulate engine setup and validation:
  - `Initialize_()` calls `CheckInitialized_()` and logs startup progress
  - `CheckInitialized_()` verifies both subsystems exist
  - `PrintWelcome_()` prints the startup banner to stdout

## Dependencies

- Depends on `graphics/Graphics.h` and `input/Input.h`.
- Uses `input/KeyMapping.h` for converting raw key input to the internal `ac::Key` enum.
- `main.cpp` creates `ac::Engine` and calls `Run()`.
- There are no dependencies on gameplay, world, or AI components at this stage.
