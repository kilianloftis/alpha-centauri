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
        TurnStageFactory[TurnStageFactory<br/>self-registering creators]
        TurnStages[TurnStages.h<br/>GlobalTurnStage<br/>PerFactionTurnStage]
        HookContext[HookContext<br/>per-stage pre/post/replace hooks]
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
        PopCompositionConfig_t[PopCompositionConfig_t]
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

    subgraph "Effects System"
        EffectConfig[EffectConfig_t<br/>EffectVariant_t<br/>scope / persistence / condition]
        ActiveEffect[ActiveEffect_t<br/>config*<br/>sourceId<br/>originBase*]
        CollectActiveEffects[CollectActiveEffects]
    end

    subgraph "Planetary Council System"
        PlanetaryCouncil[PlanetaryCouncil<br/>voting state machine]
        CouncilEffects[CouncilEffects<br/>active-effect store]
        CouncilOutcomeApplier[CouncilOutcomeApplier<br/>game mutation]
        CouncilProposalRegistry[CouncilProposalRegistry]
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
    Engine --> TurnStageFactory
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

    TurnProcessor --> TurnStages
    TurnProcessor --> GameState
    TurnStageFactory --> TurnStagesConfig
    TurnStageFactory --> TurnStages
    TurnStages --> HookContext
    TileBonusRegistry --> TileBonusConfigFile
    TileMap --> Tile
    TileBonusRegistry --> TileBonusConfig
    Tile --> TileBonusConfig
    GameState --> FactionVector
    GameState --> WorldMap
    GameDataContext --> PopTypeRegistry
    GameDataContext --> BuildingRegistry
    Building -.->|implements| IConstructable
    GameDataContext --> TechRegistry
    GameDataContext --> PopCompositionConfig_t
    GameDataContext --> PopCompositionCalculator
    GameDataContext --> LuaRuntime
    FactionFactory --> Faction
    FactionVector --> Faction
    Faction --> FactionSubsystems
    Faction --> CollectActiveEffects
    FactionSubsystems --> Tile
    GameDataContext --> EffectConfig
    BuildingRegistry --> EffectConfig

    EventBridge --> EventBus
    EventBus --> GameEvent
    Faction --> Signal
    TurnProcessor --> Signal

    GameState --> PlanetaryCouncil
    PlanetaryCouncil --> CouncilEffects
    PlanetaryCouncil --> CouncilOutcomeApplier
    PlanetaryCouncil --> CouncilProposalRegistry
    PlanetaryCouncil --> Signal
    CouncilEffects --> ActiveEffect

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
    style TurnStageFactory fill:#fbf,stroke:#333,stroke-width:2px
    style TurnStages fill:#ff9,stroke:#333,stroke-width:2px
    style GameState fill:#fbf,stroke:#333,stroke-width:3px
    style GameDataContext fill:#ffd,stroke:#333,stroke-width:3px
    style WorldMap fill:#fbf,stroke:#333,stroke-width:2px
    style PopTypeRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style BuildingRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style TechRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style PopCompositionConfig_t fill:#ffd,stroke:#333,stroke-width:2px
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
    style EffectConfig fill:#ffd,stroke:#333,stroke-width:3px
    style ActiveEffect fill:#fbf,stroke:#333,stroke-width:3px
    style CollectActiveEffects fill:#bfb,stroke:#333,stroke-width:3px
    style PlanetaryCouncil fill:#f9f,stroke:#333,stroke-width:3px
    style CouncilEffects fill:#fbf,stroke:#333,stroke-width:2px
    style CouncilOutcomeApplier fill:#fbf,stroke:#333,stroke-width:2px
    style CouncilProposalRegistry fill:#bbf,stroke:#333,stroke-width:2px
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
- **Purpose**: Manages turn-based game logic and modding hooks. See
  `docs/architecture/turn-system.md` for the detailed diagram.
- **Components**:
  - `TurnProcessor`: Dispatches each configured stage in order — once for `GlobalTurnStage`s,
    once per faction for `PerFactionTurnStage`s — and throws if a stage id has no
    registered instance
  - `TurnStageFactory`: Builds stage instances from parsed config via a self-registering
    creator registry (`TurnStageRegistrar<T>`), falling back to `CustomGlobalTurnStage`/
    `CustomPerFactionTurnStage` for mod-defined ids
  - `TurnStages`: Defines `TurnStageBase` (hook lifecycle) and the two stage interfaces,
    `GlobalTurnStage` and `PerFactionTurnStage`
  - `HookContext`/`Hook_t`: Per-stage pre/post/replace hooks, parsed from config
- **Dependencies**:
  - TurnProcessor depends on TurnStages and GameState
  - TurnStageFactory depends on TurnStages and config/turn_stages.json

### GameDataContext
- **Purpose**: Holds the definition data loaded once at startup (registries, config structs) plus the calculators/services built from it. Deliberately excludes anything that reads live save-game state — see `SecretProjectAvailabilityCalculator` below, which lives on `GameState` instead.
- **Passed whole, not unpacked**: `Faction` takes `const GameDataContext&` at construction and holds it for its lifetime, rather than receiving each registry and calculator as a separate parameter. This is what keeps new kinds of shared game data from having to be threaded through every intermediate constructor: `MoraleCalculator` (a stateless view over `moraleConfig`, owned here beside `TechCostCalculator`) reaches `Unit` via `Faction` → `UnitManager` without appearing in any create/transfer signature — `UnitManager::CreateUnit` and `Faction::TransferUnitTo` take no morale argument. `GameState` likewise borrows the calculator instead of owning one. `BaseManager` is the remaining exception: `Faction::CreateBase` still unpacks six fields to build it.
- **Outlives all live state**: every `Faction`, `BaseManager`, and `TileEffectsContext` holds non-owning references into this object, so `Engine` declares `m_gameDataContext` *before* `m_pGameState`. Members are destroyed in reverse declaration order, so the whole faction/base/unit graph is torn down while the definition data is still alive.
- **Components**:
  - `IConstructable`: Abstract interface for entities that can be constructed in a base; exposes `GetId()`, `GetName()`, and `GetMineralCost()`
  - `BuildingRegistry`: All building definitions loaded from `config/buildings.json`; each entry may have `secret_project: true` to mark it as a Secret Project
  - `TechRegistry`: All tech definitions loaded from `config/techs.json`
  - `PopTypeRegistry`: All pop type definitions loaded from `config/pop_types.json`
  - `PopCompositionConfig_t`: Composition formula config loaded via Lua
  - `PopCompositionCalculator`: Evaluates composition formulas at runtime
  - `LuaRuntime`: Shared Lua state used to load and evaluate config scripts
- **Note**: Implemented as a plain struct with public `unique_ptr` members (no getters/setters needed)

### Faction System
- **Purpose**: Manages all factions and their mutable save-game state
- **Components**:
  - `GameState`: Owns FactionVector, missionYear, and WorldMap — mutable data written to and read from disk. Also owns two world-scoped resolvers that must share the map's lifetime rather than GameDataContext's: `TileEffectsContext` (bundles the live WorldMap with the immutable ImprovementRegistry to resolve tile effects) and the stateless `UnitOrderExecutor`. `SecretProjectAvailabilityCalculator` lives here too, since it scans the live faction vector — as an owned member of the object it queries, it cannot dangle the way a `GameDataContext`-owned reference into it could. `GameState` is also the sole owner of faction/base ID allocation, via two `IdAllocator` (`lib/IdAllocator.h`) members — the only place either ID namespace is minted, so any future runtime faction/base creation (not just Engine's composition root) has somewhere to get a unique ID from. `GetPlayerFaction()` returns whichever `Faction` has `IsPlayerControlled() == true` (set at construction), not an index-0 convention — see the `Faction` bullet below. `GameState` borrows (but does not own) the `MoraleCalculator` — see the `GameDataContext` note below.
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
  - `Tile`: Represents a single map tile with position (x,y), terrain characteristics (Moisture_t, Rockiness_t, Elevation), Rivers, Landmarks, Improvements, Bonus, and Worker assignment tracking
  - `TileMap`: (Future) Container for the 2D grid of tiles
  - `TileBonusRegistry`: Loads and provides access to tile bonus definitions
  - `TileBonusConfig`: Data structure for bonus definitions (resource bonuses + sprite path)
- **Dependencies**:
  - Faction subsystems (particularly Military with Bases) work tiles for resources
  - TileBonusRegistry loads from config/tile_bonuses.json
- **Details**: See `docs/architecture/map-system.md` for detailed architecture

### Unit Movement System
- **Purpose**: Tile entry costs, step legality, path planning, and move-order execution
- **Components**:
  - `MoveCostCalculator`: Single home of the tile-entry rules — resolves a unit + tile into `EntryTerms_t` (fragment cost, fungus full-cost banking, forced end-of-turn) and a shroud-aware planning weight
  - `StepEvaluator`: Edge legality (adjacency, terrain domain, occupants, ZOC) at objective or faction-known knowledge levels
  - `Pathfinder`: Dijkstra over planned fragment costs and plannable steps
  - `UnitOrderExecutor`: Executes unit orders; spends fragments and banks multi-turn fungus charges per `EntryTerms_t`
- **Dependencies**:
  - GameState owns UnitOrderExecutor; all four bind the live WorldMap
  - MoveCostCalculator reads ImprovementRegistry configs (move_cost / move_cost_override)
- **Details**: See `docs/architecture/unit-movement-system.md` for detailed architecture

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

### Effects System
- **Purpose**: Defines and collects active bonuses and modifiers from buildings and social engineering.
- **Components**:
  - `EffectConfig_t`: Static effect definition containing a typed variant, scope, persistence, and condition.
  - `EffectVariant_t`: `std::variant` of all concrete effect structs such as `GrantBuildingEffect_t` and `StatModifierEffect_t`.
  - `ActiveEffect_t`: Runtime instance pointing back to an `EffectConfig_t`, with a source id and optional origin base.
  - `CollectActiveEffects`: Gathers all active effects for a faction by walking bases/buildings and social engineering selections.
- **Dependencies**:
  - `EffectConfig_t` is stored inside `BuildingConfig_t` and will eventually be stored in `SocialPolicyConfig_t`.
  - `CollectActiveEffects` reads from `Faction` (bases and social engineering manager).
- **Details**: See `docs/architecture/effects-system.md` for detailed architecture

### Planetary Council System
- **Purpose**: Runtime Planetary Council — proposals, voting, the Planetary Governor, and the continuous world law the council keeps in force.
- **Components**:
  - `PlanetaryCouncil`: The vote lifecycle (propose → vote/veto → resolve), fixed membership, governorship, and active-proposal set. Owned by `GameState` (`std::unique_ptr`); exposes `OnProposalOpened` / `OnResolved` signals for the UI.
  - `CouncilEffects`: Store for the continuous `ActiveEffect_t`s the council projects — world-global effects from proposals in force, plus the governor's faction-global effects.
  - `CouncilOutcomeApplier`: Applies a passed proposal's outward mutations (energy grants, governor infiltration); world-parameter outcomes are deferred to `WorldEvents`.
  - `CouncilProposalRegistry`: Proposal definitions loaded/validated from `config/council/`.
- **Dependencies**:
  - `GameState` owns the council and folds `CouncilEffects` output into the faction effect pool
  - Reads `Faction` population/effects, `DiplomacyLedger` (commlinks/infiltration), and `ResearchManager` (tech gating)
- **Details**: See `docs/architecture/council-system.md` for detailed architecture

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
