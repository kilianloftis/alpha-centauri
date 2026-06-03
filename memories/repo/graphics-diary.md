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
  - `Initialize()`
  - `Clear()`
  - `Display()`
  - `LoadTexture()`
  - `DrawSprite()`
  - `DrawText()`
- `SFMLGraphics` stores:
  - `sf::RenderWindow m_window`
  - `sf::Font m_font`
  - `std::unordered_map<std::string, sf::Texture> m_textures`
- Event processing is handled inside `SFMLGraphics::Display()` through `ProcessEvents_()`, which maps SFML key presses into the internal input queue.
- `NullGraphics` is used as a fallback when SFML is not available.

## Dependencies

- `SFMLGraphics` depends on SFML and the `input/KeyMapping.h` mapping helpers.
- The engine calls `Clear()`, `DrawText()`, and `Display()` each frame.
- No higher-level UI or rendering systems are implemented yet.
