# High-Level Architecture

```mermaid
graph TB
    subgraph "Main Entry"
        main[main.cpp]
    end

    subgraph "Game Engine"
        Engine[Engine]
    end

    subgraph "Graphics System"
        Graphics[Graphics<br/>(abstract)]
        SFMLGraphics[SFMLGraphics]
        NullGraphics[NullGraphics]
    end

    subgraph "Input System"
        Input[Input<br/>(abstract)]
        SFMLInput[SFMLInput]
        NullInput[NullInput]
        KeyMapping[KeyMapping]
        SFMLKeyEventQueue[SFMLKeyEventQueue]
    end

    subgraph "Turn System"
        TurnProcessor[TurnProcessor]
        HookSystem[HookSystem]
        TurnStageFactory[TurnStageFactory]
        TurnStages[TurnStages.h<br/>TurnStage enum<br/>TurnStageBase]
    end

    subgraph "Map System"
        Tile[Tile]
        TileMap[(TileMap<br/>future)]
        TileBonusRegistry[TileBonusRegistry]
        TileBonusConfig[TileBonusConfig]
    end

    subgraph "Faction System"
        GameState[GameState]
        FactionVector[FactionVector<br/>vector<unique_ptr<Faction>>]
        FactionFactory[FactionFactory]
        Faction[Faction]
        FactionSubsystems[Faction Subsystems:<br/>FactionIdentity, AIProfile,<br/>Economy, Military,<br/>Research, Diplomacy]
    end

    subgraph "Event System"
        Signal[Signal&lt;T&gt;<br/>Internal signals]
        EventBus[EventBus<br/>Mod-facing]
        EventBridge[EventBridge<br/>Signal→EventBus bridge]
        GameEvent[GameEvent<br/>std::variant]
    end

    subgraph "Configuration"
        TurnStagesConfig[config/turn_stages.json]
        TileBonusConfigFile[config/tile_bonuses.json]
    end

    subgraph "UI Components"
        PopulationDisplay[PopulationDisplay]
        WorldDisplay[WorldDisplay]
        BaseWorkableAreaDisplay[BaseWorkableAreaDisplay]
    end

    main --> Engine
    Engine --> GameState
    Engine --> Graphics
    Engine --> Input
    Engine --> HookSystem
    Engine --> TurnProcessor
    Engine --> EventBridge

    Graphics --> SFMLGraphics
    Graphics --> NullGraphics

    Input --> SFMLInput
    Input --> NullInput
    SFMLInput --> KeyMapping
    SFMLInput --> SFMLKeyEventQueue

    TurnProcessor --> HookSystem
    TurnProcessor --> GameState
    HookSystem --> TurnStagesConfig
    TurnStageFactory --> HookSystem
    TurnStageFactory --> TurnStagesConfig
    TileBonusRegistry --> TileBonusConfigFile
    TurnStageFactory --> TurnStages
    TileMap --> Tile
    TileBonusRegistry --> TileBonusConfig
    Tile --> TileBonusConfig
    GameState --> FactionVector
    FactionFactory --> Faction
    FactionVector --> Faction
    Faction --> FactionSubsystems
    FactionSubsystems --> Tile

    EventBridge --> GameState
    EventBridge --> EventBus
    EventBus --> GameEvent
    Faction --> Signal
    TurnProcessor --> Signal

    PopulationDisplay --> Graphics
    PopulationDisplay --> EventBus
    WorldDisplay --> Graphics
    WorldDisplay --> Tile
    BaseWorkableAreaDisplay --> Graphics
    BaseWorkableAreaDisplay --> Tile
    BaseWorkableAreaDisplay --> Base

    style Engine fill:#f9f,stroke:#333,stroke-width:4px
    style Graphics fill:#bbf,stroke:#333,stroke-width:2px
    style Input fill:#bbf,stroke:#333,stroke-width:2px
    style TurnProcessor fill:#bfb,stroke:#333,stroke-width:2px
    style HookSystem fill:#bfb,stroke:#333,stroke-width:2px
    style TurnStageFactory fill:#fbf,stroke:#333,stroke-width:2px
    style TurnStages fill:#ff9,stroke:#333,stroke-width:2px
    style GameState fill:#fbf,stroke:#333,stroke-width:3px
    style FactionVector fill:#fbf,stroke:#333,stroke-width:2px
    style FactionFactory fill:#ff9,stroke:#333,stroke-width:2px
    style Faction fill:#f9f,stroke:#333,stroke-width:2px
    style Signal fill:#f9f,stroke:#333,stroke-width:2px
    style Tile fill:#fbf,stroke:#333,stroke-width:2px
    style TileMap fill:#fbf,stroke:#333,stroke-width:2px
    style TileBonusRegistry fill:#fbf,stroke:#333,stroke-width:2px
    style TileBonusConfig fill:#ff9,stroke:#333,stroke-width:2px
    style EventBus fill:#bbf,stroke:#333,stroke-width:3px
    style EventBridge fill:#fbf,stroke:#333,stroke-width:2px
    style PopulationDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style WorldDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style BaseWorkableAreaDisplay fill:#bfb,stroke:#333,stroke-width:2px
```

## Component Overview

### Engine
- **Purpose**: Main game engine that coordinates all subsystems
- **Responsibilities**:
  - Initialize and manage game loop
  - Own and coordinate Graphics, Input, HookSystem, TurnProcessor, EventBridge, and GameState
  - Delegates all game state to GameState; emits `on_turn_started` before each turn

### Graphics System
- **Purpose**: Abstract graphics rendering interface
- **Components**:
  - `Graphics`: Abstract base class defining graphics operations
  - `SFMLGraphics`: SFML-based implementation
  - `NullGraphics`: Null implementation for testing/headless mode
- **Factory**: `CreateGraphics()` function creates appropriate implementation

### Input System
- **Purpose**: Abstract input handling interface
- **Components**:
  - `Input`: Abstract base class defining input operations
  - `SFMLInput`: SFML-based implementation
  - `NullInput`: Null implementation for testing/headless mode
  - `KeyMapping`: Maps keys to game actions
  - `SFMLKeyEventQueue`: Queues SFML key events
- **Factory**: `CreateInput()` function creates appropriate implementation

### Turn System
- **Purpose**: Manages turn-based game logic and modding hooks
- **Components**:
  - `TurnProcessor`: Processes game turns, manages faction turns
  - `HookSystem`: Manages mod hooks and stage execution
  - `TurnStageFactory`: Creates and validates turn stage instances from configuration
  - `TurnStages`: Defines TurnStage enum and TurnStageBase interface
- **Dependencies**:
  - TurnProcessor depends on HookSystem and GameState
  - TurnStageFactory depends on HookSystem and TurnStages
  - Both HookSystem and TurnStageFactory load from config/turn_stages.json

### Faction System
- **Purpose**: Manages all factions and their game state
- **Components**:
  - `GameState`: Owns FactionVector, missionYear, bShouldExit, and `on_turn_started` signal
  - `FactionVector`: Vector of unique_ptr<Faction> stored inside GameState
  - `FactionFactory`: Creates Faction instances from configuration
  - `Faction`: Represents a single faction with all its subsystems
  - `Faction Subsystems`: FactionIdentity, AIProfile, Economy, Military, Research, Diplomacy
- **Dependencies**:
  - Engine owns GameState
  - GameState owns FactionVector
  - TurnProcessor accesses FactionVector via GameState
  - FactionFactory creates Faction instances during initialization
  - Each Faction owns its subsystems
- **Details**: See `docs/architecture/faction-system.md` for detailed architecture

### Map System
- **Purpose**: Manages game world terrain and tile-based resource production
- **Components**:
  - `Tile`: Represents a single map tile with position (x,y), terrain characteristics (Moisture, Rockiness, Elevation), Rivers, Landmarks, Improvements, Bonus, and Worker assignment tracking
  - `TileMap`: (Future) Container for the 2D grid of tiles
  - `TileBonusRegistry`: Loads and provides access to tile bonus definitions
  - `TileBonusConfig`: Data structure for bonus definitions (resource bonuses + sprite path)
- **Dependencies**:
  - Faction subsystems (particularly Military with Bases) work tiles for resources
  - TileBonusRegistry loads from config/tile_bonuses.json
- **Details**: See `docs/architecture/map-system.md` for detailed architecture

### Configuration
- **Turn Stages Config**: `config/turn_stages.json` - Loaded by HookSystem to define turn stages and hooks
- **Tile Bonus Config**: `config/tile_bonuses.json` - Loaded by TileBonusRegistry to define tile bonus types and their resource bonuses

### Event System
- **Purpose**: Two-layer event system for internal engine communication and mod interface
- **Components**:
  - `Signal<T>`: Templated signal/slot for internal engine-only communication
  - `EventBus`: Mod-facing event bus with stable ABI using std::variant
  - `EventBridge`: Bridges internal signals to EventBus for mod consumption
  - `GameEvent`: std::variant type containing all mod-accessible events
- **Dependencies**:
  - Engine owns EventBridge
  - EventBridge depends on GameState and EventBus
  - Faction and TurnProcessor use Signal<T> for internal communication
- **Details**: See `docs/architecture/event-system.md` for detailed architecture

### UI Components
- **Purpose**: Display components that render game information using the Graphics interface
- **Components**:
  - `PopulationDisplay`: Displays current population and per-pop type breakdown
  - `WorldDisplay`: Displays the world map as a grid of tiles with terrain info
  - `BaseWorkableAreaDisplay`: Displays the 21-tile workable area around a base with resource production
- **Dependencies**:
  - All UI components depend on Graphics for rendering
  - PopulationDisplay subscribes to EventBus for population change events
  - WorldDisplay reads from Tile objects for terrain data
- **Details**: See `docs/architecture/graphics-system.md` for detailed UI component documentation
