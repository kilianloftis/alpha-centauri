# Input Component Current State

## Current State

- The input layer abstracts keyboard and mouse capture through `ac::Input`.
- Implementations include:
  - `NullInput` for console-based input via `std::cin`
  - `SFMLInput` when `USE_SFML` is enabled
- Synchronous input capture is available through `CaptureKey()` and `CaptureMouse()`.
- Asynchronous callbacks are supported through `CaptureKeyAsync()` and `CaptureMouseAsync()`.
- The current application primarily uses key capture to detect `Enter` and display the last pressed key.

## Architecture

- `Input` is an abstract interface with methods for initialization and both synchronous and asynchronous event capture.
- `NullInput` reads text from the console and maps raw characters to `ac::Key` values.
- `SFMLInput` uses SFML keyboard and mouse polling plus a pending key event queue stored in `SFMLKeyEventQueue`.
- `KeyMapping` converts platform-specific input values into the shared `ac::Key` enum.

## Dependencies

- `SFMLInput` depends on SFML when built with `USE_SFML`.
- `NullInput` depends only on standard I/O.
- The engine relies on this layer for key events and does not yet use mouse input in normal runtime.
