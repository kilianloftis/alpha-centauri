# Graphics System Architecture

```mermaid
graph TB
    subgraph "Graphics Interface"
        Graphics[Graphics<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>Initialize()<br/>Clear()<br/>Display()<br/>LoadTexture()<br/>DrawSprite()<br/>DrawText()]
    end

    subgraph "SFML Implementation"
        SFMLGraphics[SFMLGraphics]
        SFMLWindow[sf::RenderWindow]
        SFMLFont[sf::Font]
        TextureMap[unordered_map<string, sf::Texture>]
        EventProcessing[ProcessEvents_()]
    end

    subgraph "Null Implementation"
        NullGraphics[NullGraphics]
        ConsoleOutput[Console logging only<br/>No rendering]
    end

    subgraph "Factory"
        Factory[CreateGraphics()]
        CompileFlag[USE_SFML<br/>compile-time flag]
    end

    subgraph "Dependencies"
        KeyMapping[KeyMapping]
        SFMLKeyEventQueue[SFMLKeyEventQueue]
    end

    Graphics --> Methods
    SFMLGraphics -.->|implements| Graphics
    NullGraphics -.->|implements| Graphics

    SFMLGraphics --> SFMLWindow
    SFMLGraphics --> SFMLFont
    SFMLGraphics --> TextureMap
    SFMLGraphics --> EventProcessing

    EventProcessing --> KeyMapping
    EventProcessing --> SFMLKeyEventQueue

    Factory -->|if USE_SFML defined| SFMLGraphics
    Factory -->|if USE_SFML not defined| NullGraphics
    Factory --> CompileFlag

    NullGraphics --> ConsoleOutput

    style Graphics fill:#bbf,stroke:#333,stroke-width:4px
    style SFMLGraphics fill:#bfb,stroke:#333,stroke-width:2px
    style NullGraphics fill:#fbb,stroke:#333,stroke-width:2px
    style Factory fill:#ff9,stroke:#333,stroke-width:2px
```

## Component Overview

### Graphics (Abstract Base Class)
- **Purpose**: Defines the interface for graphics rendering operations
- **Virtual Methods**:
  - `Initialize()`: Initialize the graphics backend
  - `Clear()`: Clear the render surface
  - `Display()`: Present the rendered frame
  - `LoadTexture(id, path)`: Load a texture from file
  - `DrawSprite(textureId, x, y)`: Draw a sprite at position
  - `DrawText(text, x, y, size)`: Draw text at position

### SFMLGraphics
- **Purpose**: SFML-based graphics implementation
- **Components**:
  - `sf::RenderWindow`: SFML render window (800x600, 60 FPS)
  - `sf::Font`: Font for text rendering (DejaVuSans or LiberationSans)
  - `unordered_map<string, sf::Texture>`: Texture cache
  - `ProcessEvents_()`: Processes SFML events and forwards key events
- **Dependencies**:
  - Uses `KeyMapping` to convert SFML keys to internal Key enum
  - Uses `SFMLKeyEventQueue` to queue key events for input system
- **Behavior**:
  - Ignores window close button (only Enter should close)
  - Loads fonts from system paths with fallback options

### NullGraphics
- **Purpose**: Null graphics implementation for testing/headless mode
- **Behavior**:
  - All methods are no-ops or log to console
  - Used when `USE_SFML` is not defined
  - Allows game logic testing without graphics

### CreateGraphics() Factory
- **Purpose**: Factory function to create appropriate graphics implementation
- **Selection**: Based on `USE_SFML` compile-time flag
  - If defined: Returns `SFMLGraphics`
  - If not defined: Returns `NullGraphics`
