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

    subgraph "UI System"
        UIManager[UIManager<br/>(abstract)]
        UIManagerImpl[UIManagerImpl]
        IGameView[IGameView<br/>(interface)]
        UIElements[UIWorldMap, UIPanel, UIPopup<br/>extends UIElement]
        Views[WorldView, BaseView, ResearchView<br/>implement IGameView]
        ViewFactory[ViewFactory]
        IBasePanel[IBasePanel<br/>interface for base panels]
        BasePanels[BaseDisplay, BaseWorkableAreaDisplay,<br/>PopulationDisplay, GrowthDisplay<br/>implement IBasePanel]
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

    subgraph "GameDataContext (immutable definition data)"
        PopTypeRegistry[PopTypeRegistry]
        BuildingRegistry[BuildingRegistry]
        TechRegistry[TechRegistry]
        PopCompositionConfig[PopCompositionConfig]
        PopCompositionCalculator[PopCompositionCalculator]
        LuaRuntime[LuaRuntime]
    end

    subgraph "Faction System"
        GameState[GameState<br/>(mutable save-game data)]
        WorldMap[WorldMap]
        FactionVector[FactionVector<br/>vector&lt;unique_ptr&lt;Faction&gt;&gt;]
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
        BaseDisplay[BaseDisplay]
        PopulationDisplay[PopulationDisplay]
        WorldDisplay[WorldDisplay]
        BaseWorkableAreaDisplay[BaseWorkableAreaDisplay]
    end

    main --> Engine
    Engine --> GameState
    Engine --> GameDataContext
    Engine --> Graphics
    Engine --> Input
    Engine --> HookSystem
    Engine --> TurnProcessor
    Engine --> UIManager
    Engine --> ViewFactory
    Engine --> EventBridge

    UIManagerImpl -.->|implements| UIManager
    UIManager -->|manages stack of| IGameView
    IGameView --> UIElements
    Views -.->|implement| IGameView
    ViewFactory -->|creates| Views
    UIManagerImpl --> Graphics
    UIManagerImpl --> Input

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
    GameState --> WorldMap
    GameDataContext --> PopTypeRegistry
    GameDataContext --> BuildingRegistry
    GameDataContext --> TechRegistry
    GameDataContext --> PopCompositionConfig
    GameDataContext --> PopCompositionCalculator
    GameDataContext --> LuaRuntime
    FactionFactory --> Faction
    FactionVector --> Faction
    Faction --> FactionSubsystems
    FactionSubsystems --> Tile

    EventBridge --> EventBus
    EventBus --> GameEvent
    Faction --> Signal
    TurnProcessor --> Signal

    BaseDisplay --> Graphics
    BaseDisplay --> Base
    PopulationDisplay --> Graphics
    PopulationDisplay --> EventBus
    WorldDisplay --> Graphics
    WorldDisplay --> Tile
    BaseWorkableAreaDisplay --> Graphics
    BaseWorkableAreaDisplay --> Tile
    BaseWorkableAreaDisplay --> Base
    Views --> IBasePanel
    BasePanels -.->|implement| IBasePanel

    style Engine fill:#f9f,stroke:#333,stroke-width:4px
    style Graphics fill:#bbf,stroke:#333,stroke-width:2px
    style Input fill:#bbf,stroke:#333,stroke-width:2px
    style TurnProcessor fill:#bfb,stroke:#333,stroke-width:2px
    style HookSystem fill:#bfb,stroke:#333,stroke-width:2px
    style TurnStageFactory fill:#fbf,stroke:#333,stroke-width:2px
    style TurnStages fill:#ff9,stroke:#333,stroke-width:2px
    style GameState fill:#fbf,stroke:#333,stroke-width:3px
    style GameDataContext fill:#ffd,stroke:#333,stroke-width:3px
    style WorldMap fill:#fbf,stroke:#333,stroke-width:2px
    style PopTypeRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style BuildingRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style TechRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style PopCompositionConfig fill:#ffd,stroke:#333,stroke-width:2px
    style PopCompositionCalculator fill:#ffd,stroke:#333,stroke-width:2px
    style LuaRuntime fill:#ffd,stroke:#333,stroke-width:2px
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
    style BaseDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style PopulationDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style WorldDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style BaseWorkableAreaDisplay fill:#bfb,stroke:#333,stroke-width:2px
    style IBasePanel fill:#bbf,stroke:#333,stroke-width:2px
    style UIManager fill:#bbf,stroke:#333,stroke-width:2px
    style ViewFactory fill:#ff9,stroke:#333,stroke-width:2px
```

## Component Overview

### Engine
- **Purpose**: Main game engine that coordinates all subsystems
- **Responsibilities**:
  - Initialize and manage game loop
  - Own and coordinate Graphics, Input, HookSystem, TurnProcessor, EventBridge, GameState, and ViewFactory
  - Owns `m_bShouldExit`; publishes `EvTurnStarted` directly to `EventBus` each turn

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

### GameDataContext
- **Purpose**: Holds all immutable definition data loaded once at startup; never serialised
- **Components**:
  - `BuildingRegistry`: All building definitions loaded from `config/buildings.json`
  - `TechRegistry`: All tech definitions loaded from `config/techs.json`
  - `PopTypeRegistry`: All pop type definitions loaded from `config/pop_types.json`
  - `PopCompositionConfig`: Composition formula config loaded via Lua
  - `PopCompositionCalculator`: Evaluates composition formulas at runtime
  - `LuaRuntime`: Shared Lua state used to load and evaluate config scripts
- **Note**: Implemented as a plain struct with public `unique_ptr` members (no getters/setters needed)

### Faction System
- **Purpose**: Manages all factions and their mutable save-game state
- **Components**:
  - `GameState`: Owns FactionVector, missionYear, and WorldMap — mutable data written to and read from disk; no registries or calculators
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

### UI System
- **Purpose**: Abstract UI management with layered rendering
- **Components**:
  - `UIManager`: Abstract base class managing UI elements
  - `SFMLUIManager`: SFML-based implementation
  - `NullUIManager`: Null implementation for testing/headless mode
  - `UIElement`: Abstract base for all UI elements (position, size, visibility)
  - `UIWorldMap`: World map layer (bottom)
  - `UIPanel`: Information panel at screen bottom
  - `UIPopup`: Modal popup with dismiss button
  - `ViewFactory`: Creates game views from game state and graphics context
  - `ResearchView`: Research overlay view
- **Factory**: `CreateUIManager()` function creates appropriate implementation
- **Details**: See `docs/architecture/ui-system.md` for detailed architecture

### Configuration
- **Turn Stages Config**: `config/turn_stages.json` - Loaded by HookSystem to define turn stages and hooks
- **Tile Bonus Config**: `config/tile_bonuses.json` - Loaded by TileBonusRegistry to define tile bonus types and their resource bonuses
- **Tech Config**: `config/techs.json` - Loaded by TechRegistry to define available technologies, their costs, and unlock chains

### Event System
- **Purpose**: Two-layer event system for internal engine communication and mod interface
- **Components**:
  - `Signal<T>`: Templated signal/slot for internal engine-only communication
  - `EventBus`: Mod-facing event bus with stable ABI using std::variant
  - `EventBridge`: Bridges internal signals to EventBus for mod consumption
  - `GameEvent`: std::variant type containing all mod-accessible events
- **Dependencies**:
  - Engine owns EventBridge
  - EventBridge depends on EventBus only (GameState wiring added per-subsystem via `WireBase` etc.)
  - Faction and TurnProcessor use Signal<T> for internal communication
- **Details**: See `docs/architecture/event-system.md` for detailed architecture

### UI Components
- **Purpose**: Display components that render game information using the Graphics interface
- **Components**:
  - `IBasePanel`: Interface for panels coordinated by `BaseView`
  - `BaseDisplay`: Displays base name, resource stockpiles, and click status text
  - `PopulationDisplay`: Displays current population and per-pop type breakdown
  - `GrowthDisplay`: Displays nutrient stockpile, growth threshold, and nutrient production
  - `WorldDisplay`: Displays the world map as a grid of tiles with terrain info
  - `BaseWorkableAreaDisplay`: Displays the 21-tile workable area around a base with resource production
- **Dependencies**:
  - All UI components depend on Graphics for rendering
  - PopulationDisplay subscribes to EventBus for population change events
  - WorldDisplay reads from Tile objects for terrain data
  - `BaseView` coordinates `BaseDisplay`, `BaseWorkableAreaDisplay`, `PopulationDisplay`, and `GrowthDisplay` via `IBasePanel`
- **Details**: See `docs/architecture/graphics-system.md` for detailed UI component documentation
