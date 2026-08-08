# Graphics System Architecture

```mermaid
graph TB
    subgraph "Graphics Interface"
        Graphics[Graphics<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>PumpEvents()<br/>Clear()<br/>Display()<br/>LoadTexture()<br/>DrawSprite()<br/>DrawText()]
    end

    subgraph "SFML Implementation"
        SFMLGraphics[SFMLGraphics]
        SFMLWindow[sf::RenderWindow]
        SFMLFont[sf::Font]
        TextureMap[unordered_map<string, sf::Texture>]
        EventProcessing[PumpEvents → PlatformEventQueue]
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
        PlatformEventQueue[PlatformEventQueue<br/>owned by Engine]
    end

    Graphics --> Methods
    SFMLGraphics -.->|implements| Graphics
    NullGraphics -.->|implements| Graphics

    SFMLGraphics --> SFMLWindow
    SFMLGraphics --> SFMLFont
    SFMLGraphics --> TextureMap
    SFMLGraphics --> EventProcessing

    EventProcessing --> KeyMapping
    EventProcessing --> PlatformEventQueue

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
  - `PumpEvents()`: Drain the window's event queue into the `PlatformEventQueue` this backend was constructed with. Deliberately **not** part of `Display()`: pumping used to hang off the render call, which made rendering a prerequisite for receiving a keystroke and gave a draw call hidden I/O side effects.
  - `Clear()`: Clear the render surface
  - `Display()`: Present the rendered frame
  - `LoadTexture(id, path)`: Load a texture from file
  - `DrawSprite(textureId, x, y)`: Draw a sprite at position
  - `DrawText(text, x, y, size)`: Draw text at position
  - `DrawRect(x, y, width, height, color, thickness)`: Draw an outline rectangle (negative thickness draws inward)

### SFMLGraphics
- **Purpose**: SFML-based graphics implementation
- **Components**:
  - `sf::RenderWindow`: SFML render window, sized/titled/FPS-capped from `GraphicsConfig_t`
  - `sf::Font`: opened from the first usable path in `GraphicsConfig_t::fontPaths`. **Throws if none opens** — the entire UI is text and rectangles, so "no font" would present a black window with no diagnostic.
  - `unordered_map<string, sf::Texture>`: Texture cache; `LoadTexture` replaces an existing id
  - `PumpEvents()`: Translates SFML events and pushes them onto the shared `PlatformEventQueue`
- **Dependencies**:
  - Uses `KeyMapping` (SFML half in `SfmlKeyMapping.cpp`) to convert SFML keys to `Key_t`
  - Writes to `PlatformEventQueue`; it never names an `Input` implementation
- **Window close**: a close request is *recorded* on the `PlatformEventQueue`, not acted on. `Engine::GameLoop_` takes it and calls `UIManager::RequestExit()`, so every quit path sets the same flag. What closing means is the engine's decision, not the backend's.

### NullGraphics
- **Purpose**: Substitutable no-op backend for headless runs
- **Behavior**:
  - Draw calls do nothing and **report success**: a caller should not have to special-case headless to tell "did nothing" from "went wrong"
  - `PumpEvents()` writes nothing, so with no other producer the queue stays empty and `Input` polls empty — never blocking
  - `Display()` **paces the frame loop** to `GraphicsConfig_t::framerateLimit`. SFML's `setFramerateLimit` is the only pacing in the SFML build; without an equivalent here a headless run would spin at 100% CPU
  - Reports the same window size as the SFML backend, from the shared `GraphicsConfig_t`
  - Used when `USE_SFML` is not defined

### CreateGraphics() Factory
- **Purpose**: Factory function to create appropriate graphics implementation
- **Selection**: Based on `USE_SFML` compile-time flag
  - If defined: Returns `SFMLGraphics`
  - If not defined: Returns `NullGraphics`

## UI Components

UI components use the Graphics interface to render game information.

### PopulationDisplay
- **Purpose**: Displays current population and per-pop type breakdown
- **File**: `ui/PopulationDisplay.h`, `ui/PopulationDisplay.cpp`
- **Dependencies**: EventBus (for population change events), Graphics
- **Methods**:
  - `Render(x, y)`: Render at specified position
  - `SetPopulation()`: Set the population manager to display
  - `SetCurrentPop()`: Set population directly

### WorldDisplay
- **Purpose**: Displays the world map as a grid of tiles with base markers
- **File**: `ui/world/WorldDisplay.h`, `ui/world/WorldDisplay.cpp`
- **Dependencies**: Graphics, GameState (WorldMap + factions/bases)
- **Methods**:
  - `Render(rGraphics)`: Render the visible viewport from the stored layout/camera
  - `SetSelectedUnit(pUnit)`: Highlight the player's selected unit
  - `GetViewport().SetCamera(tileX, tileY)`: Set the top-left tile of the viewport
- **Tile Display Format**: Each tile shows `moisture rockiness elevation(km)` as integers
  - Moisture_t: 0=Arid, 1=Moist, 2=Wet
  - Rockiness_t: 0=Flat, 1=Rolling, 2=Rocky
  - Elevation: Integer km (elevation in meters / 1000)
- **Architecture Note**: `WorldDisplay` reads the map and bases live from `GameState` during
  render (no per-frame base-info DTO). Base-at-tile clicks go through
  `GameState::FindBaseAt`, owned by the model rather than `WorldView`.

### BaseWorkableAreaDisplay
- **Purpose**: Displays the workable area of a base (21 tiles in 5x5 diamond pattern)
- **File**: `ui/BaseWorkableAreaDisplay.h`, `ui/BaseWorkableAreaDisplay.cpp`
- **Dependencies**: Graphics, WorldMap, Base, WorkerAssignmentManager
- **Methods**:
  - `Render(x, y, tileSize)`: Render the workable area centered at position
  - `SetBase()`: Set the base to display workable area for
- **Tile Display Format**: Each tile shows `nutrients minerals energy`
- **Visual Indicators**: 
  - Worked tiles: Green text
  - Unworked tiles: White text
  - Base center: Yellow "BASE" label

### TileHitTester
- **Purpose**: Converts pixel coordinates (e.g. mouse clicks) to world tile coordinates
- **File**: `ui/TileHitTester.h`, `ui/TileHitTester.cpp`
- **Dependencies**: None (stateless utility, all methods are static)
- **Methods**:
  - `HitTestWorldGrid(mouseX, mouseY, gridOriginX, gridOriginY, tileSize, mapWidth, mapHeight)`: Returns `optional<pair<int,int>>` tile coords for clicks on the world map grid
  - `HitTestBaseWorkableArea(mouseX, mouseY, renderCenterX, renderCenterY, tileSize, baseX, baseY)`: Returns `optional<pair<int,int>>` tile coords for clicks on the base workable area (validates diamond pattern)
- **Usage**: Shared by WorldDisplay and BaseWorkableAreaDisplay to translate mouse input into tile selection

## View System

The Engine manages a `ViewMode` state machine with two views:

### World View (default)
- Renders `WorldDisplay` with the full tile grid and yellow "BASE" labels
- **Mouse click on base tile**: Opens Base View for that base
- **Mouse click elsewhere**: Shows clicked tile coordinates
- **Enter**: Processes a turn
- **Escape**: Exits the game

### Base View
- Renders `BaseWorkableAreaDisplay` centered on the active base
- Shows the base name at the top
- **Mouse click on workable tile**: Shows clicked tile coordinates
- **Escape**: Returns to World View
