# Graphics Component Current State

## Current State

- The graphics layer provides a backend abstraction through `ac::Graphics`.
- Implementations include:
  - `NullGraphics` for a no-op backend that prints diagnostic messages
  - `SFMLGraphics` when `USE_SFML` is enabled
- Current rendering is limited to text drawing and optional sprite rendering.
- `SFMLGraphics` loads a system font and can clear/display the window, draw text, and manage textures.
- There is no scene graph, camera transform, or game world rendering system yet.

## Architecture

- `Graphics` is an abstract base class with these core methods:
  - `PumpEvents()`
  - `Clear()`
  - `Display()`
  - `LoadTexture()`
  - `DrawSprite()`
  - `DrawText()`
- `SFMLGraphics` stores:
  - `PlatformEventQueue& m_rEvents` — the queue its pump writes into
  - `sf::RenderWindow m_window`
  - `sf::Font m_font` — opened from `GraphicsConfig_t::fontPaths`; **throws** if none opens
  - `std::unordered_map<std::string, sf::Texture> m_textures`
- Event processing is `PumpEvents()`, called by `Engine::GameLoop_` once per frame and separate
  from `Display()`: a draw call must not be a prerequisite for receiving a keystroke.
- `NullGraphics` is a substitutable no-op: draws report success, and `Display()` paces the frame
  loop, since SFML's `setFramerateLimit` is the only pacing in the windowed build.
- Window size, title, FPS cap and font paths come from `GraphicsConfig_t`, so both backends
  report the same window size.

## Dependencies

- `SFMLGraphics` depends on SFML and on `input/KeyMapping.h` (SFML-only declarations).
- The engine calls `PumpEvents()`, `Clear()`, the draw methods, and `Display()` each frame.
- No higher-level UI or rendering systems are implemented yet.
