# Graphics System Architecture

```mermaid
graph TB
    subgraph "Graphics Interface"
        Graphics[Graphics<br/>(abstract base class)]
        Methods[Virtual Methods:<br/>PumpEvents()<br/>Clear()<br/>Display()<br/>LoadTexture()<br/>DrawSprite()<br/>DrawText()<br/>SetMouseCursor()<br/>ResetMouseCursor()]
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
  - `Display()`: Present the rendered frame (SFML also applies `framerateLimit` here)
  - `PaceFrame()`: Sleep to honor `framerateLimit` without presenting — used when UIManager skips a quiet frame
  - `LoadTexture(id, path)`: Load a texture from file
  - `UpsertTextureRGBA(id, width, height, rgba)`: Create or replace an RGBA8 texture from tightly packed pixels (minimap terrain cache)
  - `DrawSprite(textureId, x, y)` / `DrawSprite(..., destWidth, destHeight)` / `DrawSprite(..., destWidth, destHeight, tint)`: Draw a sprite at position, optionally scaled and color-multiplied (elevation/fog on map tiles)
  - `DrawText(text, x, y, size)`: Draw text at position
  - `DrawRect(x, y, width, height, color, thickness)`: Draw an outline rectangle (negative thickness draws inward)
  - `DrawFilledDiamond` / `DrawDiamond`: Isometric tile footprint whose AABB is `(x, y, width, height)`
  - `SetMouseCursor(path, hotspotX, hotspotY)`: Apply a custom OS cursor from an image file. Empty path or load failure leaves the current cursor and returns false; the backend keeps the cursor object alive until the next set/reset
  - `ResetMouseCursor()`: Restore the system arrow cursor

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
  - `Display()` presents; `PaceFrame()` sleeps to `GraphicsConfig_t::framerateLimit`. SFML's
    `setFramerateLimit` paces presented frames; skip-redraw paths call `PaceFrame()` instead.
    Without an equivalent here a headless run would spin at 100% CPU
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

### WorldDisplay / MapViewport
- **Purpose**: Displays the world map as a SMAC-style 2:1 isometric diamond grid with base markers
- **File**: `ui/world/WorldDisplay.h`, `ui/world/MapViewport.h`
- **Model vs presentation**: `WorldMap` stays square (neighbors, wrap-X, pathfinding). All isometric
  math lives in `MapViewport` (`PixelOriginOf` / `PixelCenterOf` / `WorldCoordsAtPixel` /
  depth-ordered `ForEachVisibleTile`).
- **Methods**:
  - `Render(rGraphics)`: Painter’s-algorithm pass over visible diamonds from the stored camera
  - `SetSelectedUnit(pUnit)`: Highlight the player's selected unit
  - `GetViewport().SetCamera(tileX, tileY)`: Anchor tile for the isometric projection
- **Tile drawing**: Each diamond is painted by `TileRenderer` — elevation-colored fill, then
  `ResolveTileLayers` sprites scaled to the diamond AABB with an elevation/fog tint. Missing
  PNGs fall back to procedural moisture/rockiness cues. Art is not shipped; run
  `extract_terrain.py` against a local SMAC install to populate `assets/sprites/` (diamond
  alpha mask applied by default). Elevation perspective / cliff skirts are a follow-on.
- **Hit-testing**: `WorldView` calls `MapViewport::WorldCoordsAtPixel`. Orthogonal
  `TileHitTester::HitTestWorldGrid` remains for non-iso grids; base workable area stays orthogonal.
- **Architecture Note**: `WorldDisplay` reads the map and bases live from `GameState` during
  render (no per-frame base-info DTO). Base-at-tile clicks go through
  `GameState::FindBaseAt`, owned by the model rather than `WorldView`.

### TileRenderer
- **Purpose**: Shared map-cell paint for the world map, location preview, and similar views
- **File**: `ui/TileRenderer.h`, `ui/TileRenderer.cpp`
- **Footprint**: 2:1 diamond (`size` = width, height = size / 2)
- **Elevation**: continuous meters → fill gradient and sprite color multiply (not a `TileLayer`)
- **Layers**: fungus wins vegetation; cliff-edge compositing deferred

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
- **Purpose**: Converts pixel coordinates to tile coordinates for orthogonal grids
- **File**: `ui/TileHitTester.h`, `ui/TileHitTester.cpp`
- **Dependencies**: None (stateless utility, all methods are static)
- **Methods**:
  - `HitTestWorldGrid(...)`: Orthogonal grid helper (world map uses `MapViewport::WorldCoordsAtPixel`)
  - `HitTestBaseWorkableArea(mouseX, mouseY, renderCenterX, renderCenterY, tileSize, baseX, baseY)`: Returns `optional<pair<int,int>>` tile coords for clicks on the base workable area (validates diamond pattern)
- **Usage**: Base workable area and similar orthogonal panels

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
