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
  - `DrawRect(x, y, width, height, color, thickness)`: Draw an outline rectangle (negative thickness draws inward)

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
- **Purpose**: Displays the world map as a grid of tiles
- **File**: `ui/WorldDisplay.h`, `ui/WorldDisplay.cpp`
- **Dependencies**: Graphics, WorldMap
- **Methods**:
  - `Render(x, y, tileSize)`: Render the grid at position with given tile size
  - `SetWorldMap()`: Set the world map to display
  - `SetBasePositions()`: Set positions to overlay yellow "BASE" labels
- **Tile Display Format**: Each tile shows `moisture rockiness elevation(km)` as integers
  - Moisture: 0=Arid, 1=Moist, 2=Wet
  - Rockiness: 0=Flat, 1=Rolling, 2=Rocky
  - Elevation: Integer km (elevation in meters / 1000)

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
