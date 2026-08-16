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
        BufferedInput[BufferedInput]
        PlatformEventQueue[(PlatformEventQueue<br/>owned by Engine)]
        SfmlKeyMapping[SfmlKeyMapping<br/>USE_SFML only]
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
        WorldMap[(WorldMap<br/>owns the tile grid)]
        UnitPositionIndex[UnitPositionIndex]
        WorkedTileIndex[WorkedTileIndex]
        TerritoryMap[TerritoryMap]
        ImprovementRegistry[ImprovementRegistry]
        ImprovementConfig[ImprovementConfig_t]
        WorldGenerator[WorldGenerator]
    end

    subgraph "GameDataContext (immutable definition data)"
        PopTypeRegistry[PopTypeRegistry]
        BuildingRegistry[BuildingRegistry]
        StockpileRegistry[StockpileRegistry]
        TechRegistry[TechRegistry]
        PopCompositionConfig_t[PopCompositionConfig_t]
        PopCompositionCalculator[PopCompositionCalculator]
        HurryProductionCalculator[HurryProductionCalculator]
        ScrapRefundCalculator[ScrapRefundCalculator]
        LuaRuntime[LuaRuntime]
    end

    subgraph "Faction System"
        GameState[GameState<br/>(mutable save-game data)]
        WorldMap[WorldMap]
        FactionVector[FactionVector<br/>vector&lt;unique_ptr&lt;Faction&gt;&gt;]
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
        ImprovementsConfigFile[config/improvements.json]
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

    Input --> BufferedInput
    BufferedInput --> PlatformEventQueue
    SFMLGraphics --> PlatformEventQueue
    SFMLGraphics --> SfmlKeyMapping

    TurnProcessor --> TurnStages
    TurnProcessor --> GameState
    TurnStageFactory --> TurnStagesConfig
    TurnStageFactory --> TurnStages
    TurnStages --> HookContext
    ImprovementRegistry --> ImprovementsConfigFile
    WorldMap --> Tile
    WorldMap --> UnitPositionIndex
    WorldMap --> WorkedTileIndex
    WorldMap --> TerritoryMap
    ImprovementRegistry --> ImprovementConfig
    Tile --> ImprovementConfig
    WorldGenerator --> WorldMap
    WorldGenerator --> ImprovementRegistry
    GameState --> FactionVector
    GameState --> WorldMap
    GameDataContext --> PopTypeRegistry
    GameDataContext --> BuildingRegistry
    GameDataContext --> StockpileRegistry
    Building -.->|implements| IConstructable
    GameDataContext --> TechRegistry
    GameDataContext --> PopCompositionConfig_t
    GameDataContext --> PopCompositionCalculator
    GameDataContext --> HurryProductionCalculator
    GameDataContext --> ScrapRefundCalculator
    HurryProductionCalculator --> LuaRuntime
    GameDataContext --> LuaRuntime
    FactionVector --> Faction
    Faction --> FactionSubsystems
    Faction --> CollectActiveEffects
    FactionSubsystems --> Tile
    GameDataContext --> EffectConfig
    BuildingRegistry --> EffectConfig
    StockpileRegistry --> EffectConfig

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
    style StockpileRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style TechRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style PopCompositionConfig_t fill:#ffd,stroke:#333,stroke-width:2px
    style PopCompositionCalculator fill:#ffd,stroke:#333,stroke-width:2px
    style LuaRuntime fill:#ffd,stroke:#333,stroke-width:2px
    style FactionVector fill:#fbf,stroke:#333,stroke-width:2px
    style Faction fill:#f9f,stroke:#333,stroke-width:2px
    style Signal fill:#f9f,stroke:#333,stroke-width:2px
    style Tile fill:#fbf,stroke:#333,stroke-width:2px
    style WorldMap fill:#fbf,stroke:#333,stroke-width:2px
    style ImprovementRegistry fill:#fbf,stroke:#333,stroke-width:2px
    style ImprovementConfig fill:#ff9,stroke:#333,stroke-width:2px
    style WorldGenerator fill:#fbf,stroke:#333,stroke-width:2px
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
  - Own and coordinate Graphics, Input, TurnProcessor, EventBridge, GameState, and ViewFactory
    (hooks are per-stage `HookContext`s owned by the stages, not an engine-level system)
  - Owns `m_bShouldExit`; publishes `EvTurnStarted` directly to `EventBus` each turn

### Graphics System
- **Purpose**: Abstract graphics rendering interface
- **Components**:
  - `Graphics`: Abstract base class defining graphics operations
  - `SFMLGraphics`: SFML-based implementation
  - `NullGraphics`: Substitutable no-op backend for headless runs; also paces the frame loop
- **Factory**: `CreateGraphics(PlatformEventQueue&, GraphicsConfig_t)` — the queue it writes into, and the presentation knobs

### Input System
- **Purpose**: Abstract input handling interface
- **Components**:
  - `Input`: Abstract base class — `PollKey` / `PollMouse` / `GetLastMousePosition`, never blocking
  - `BufferedInput`: the only implementation; reads the shared queue and names no windowing library, so it serves every backend
  - `PlatformEventQueue`: the seam between the windowing backend and `Input`, owned by `Engine`
  - `SfmlKeyMapping`: SFML→engine key/button translation, compiled only under `USE_SFML`
- **Factory**: `CreateInput(PlatformEventQueue&)`
- **Details**: See `docs/architecture/input-system.md`

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
  - `StockpileRegistry`: Never-completing production items loaded from `config/stockpiles.json` (see `config/README-stockpiles.md`). Queued like a building but never constructed: each turn `SurplusConversion` converts the base's leftover minerals through the queued entry's `MineralsConverted` modifiers. Also supplies the empty-queue default via `FindFallback`
  - `TechRegistry`: All tech definitions loaded from `config/techs.json`
  - `PopTypeRegistry`: All pop type definitions loaded from `config/pop_types.json`
  - `PopCompositionConfig_t`: Composition formula config loaded via Lua
  - `PopCompositionCalculator`: Evaluates composition formulas at runtime
  - `HurryProductionCalculator`: Prices energy-for-minerals hurrying from `production.json` `kinds.<kind>.hurry`; borrowed by every `BaseManager`
  - `ScrapRefundCalculator`: Prices player scrap from `production.json` `kinds.<kind>.scrap`; borrowed by every `BaseManager`
  - `LuaRuntime`: Shared Lua state used to load and evaluate config scripts
- **Note**: Implemented as a plain struct with public `unique_ptr` members (no getters/setters needed)
- **Valid by construction**: `LoadGameData(paths)` is a *factory* — it returns a fully populated
  context by value (the struct is move-only) rather than filling a default-constructed bag.
  `ThrowIfIncomplete` runs before it returns and throws naming the first null member, so a
  partially-loaded context cannot escape the loader. **Consumers may therefore dereference any
  member without checking**, and the subsystems that need pieces of it take them as constructor
  references rather than nullable pointers. Test fixtures deliberately assemble a narrower
  context (only what `Faction` and `BaseManager` need) and do not run the completeness check;
  the reference-typed constructors are what stop them building a half-valid object from it.

### Composition root phases
`Engine::Initialize_` runs three explicit phases, in order:
1. `InitializeApp_` — process-wide and session-independent: user settings, UI style, and
   `LoadGameData`. A future "new game" from the menu must not re-run it.
2. `StartNewGame_` — everything a session owns: the resolved session seed, world generation,
   `GameState`, factions and their starting assets, and the Planetary Council.
3. `InitializeUi_` — the turn pipeline (`turn_stages.json`) and the views, which bind to the
   session that exists by then.

**Session seed.** `StartNewGame_` resolves one seed (the map config's, or a drawn value when
that is 0), reports it, and hands it down: to `WorldGenerator::Generate`, to `GameState`'s roll
RNG, and as a per-faction sub-stream to each `Faction`. No sub-object reaches for
`std::random_device` on its own, which is what makes a session reproducible from the reported
seed. (Persisting that seed into save state is still open — see the world-generation package.)

### Faction System
- **Purpose**: Manages all factions and their mutable save-game state
- **Components**:
  - `GameState`: Owns FactionVector, missionYear, and WorldMap — mutable data written to and read from disk. Also owns two world-scoped resolvers that must share the map's lifetime rather than GameDataContext's: `TileEffectsContext` (bundles the live WorldMap with the immutable ImprovementRegistry to resolve tile effects) and the stateless `UnitOrderExecutor`. `SecretProjectAvailabilityCalculator` lives here too, since it scans the live faction vector — as an owned member of the object it queries, it cannot dangle the way a `GameDataContext`-owned reference into it could. `GameState` is also the sole owner of faction/base ID allocation, via two `IdAllocator` (`lib/IdAllocator.h`) members — the only place either ID namespace is minted, so any future runtime faction/base creation (not just Engine's composition root) has somewhere to get a unique ID from. `GetPlayerFaction()` returns whichever `Faction` has `IsPlayerControlled() == true` (set at construction), not an index-0 convention — see the `Faction` bullet below. `GameState` borrows (but does not own) the `MoraleCalculator` — see the `GameDataContext` note below.
  - `FactionVector`: Vector of unique_ptr<Faction> stored inside GameState
  - `Faction`: Represents a single faction with all its subsystems
  - `Faction Subsystems`: FactionIdentity, AIProfile, Economy, Military, Research, Diplomacy
- **Dependencies**:
  - Engine owns GameState
  - GameState owns FactionVector
  - TurnProcessor accesses FactionVector via GameState
  - `Engine::StartNewGame_` constructs factions (there is no `FactionFactory`) and registers
    them with `GameState::AddFaction`, which is registration-only — see
    `faction-system.md`, "Faction construction", for the constructor contract and the
    load-bearing attach order
  - Each Faction owns its subsystems
- **Details**: See `docs/architecture/faction-system.md` for detailed architecture

### Diplomacy and Trade
- **Purpose**: Pairwise relationships and the proposal/trade pipeline.
- **Scope**: world, not per-faction. `DiplomacyLedger` and `DiplomaticActionExecutor` live on
  `GameState`, because a relationship belongs to the pair; `Faction` owns only the state a trade
  moves (treasury, techs, explored map, bases).
- **Details**: See `docs/architecture/diplomacy-system.md`.

### Object lifetime and ownership transfer
- **Purpose**: One protocol for what "destroy" and "transfer" mean for `Unit` and `BaseManager`,
  so gameplay effects, `EventBridge`, and UI invalidation all agree on it. See
  `docs/full-review-fix-prompts/03-lifetime-and-transfer.md` for the full analysis this codifies.
- **Destroy (unit) = `UnitManager::DestroyUnit` only.** Applies combat carrier-loss cargo rules
  (a destroyed carrier's cargo that cannot survive on the exposed tile is destroyed; survivors
  disembark) and emits `OnUnitDestroyed` *before* the unit is erased, so observers (UI selection,
  `GameState` revealed-unit cleanup) can invalidate their reference while it is still valid.
  Real death only — combat loss, starvation of a unit-holding pop, etc.
- **Transfer (unit) ≠ destroy.** `Faction::TransferUnitTo` never calls `DestroyUnit`. It uses
  `UnitManager::ReleaseUnit` (removes the unit from the giver's `UnitManager` with **no** cargo
  loss and **no** `OnUnitDestroyed` — emits `OnUnitReleased` instead) followed by
  `UnitManager::AdoptUnit` on the receiver (rebinds the unit's `Faction*` via `Unit::RebindFaction`,
  adds it to the receiver's `UnitManager`, emits `OnUnitAdopted`). A transferred carrier's entire
  embarked cargo graph moves with it, still embarked, under the new owner. A unit transferred
  while embarked on someone else's carrier is disembarked cleanly first (no destroy, no
  `CanPlaceUnitOnTile` conflict against the carrier's own tile), then transferred. Home-base
  claims do not follow a transferred unit to a foreign faction's base — cleared on transfer, not
  reassigned; the produced-at record is cleared for the same reason (it names a base of the
  previous owner, and keeping it would let the unit claim `ProducedAtThisBase` bonuses if the new
  owner ever captured that base). Mods/observers see one adopt event, never a fake
  death-then-birth pair.
- **Destroy (base) = raze / extract-and-destroy (`Faction::ExtractBase`).** The `BaseManager`
  object dies: `~BaseManager` emits `OnDestroyed` (while still fully valid, so an open `BaseView`
  can pop before the reference dangles), `HomeBaseIndex` orphans every claim into the base
  (units keep existing, just lose that home), deploy-cooldown records for that `baseId` are
  dropped (`Faction::DropBuildingDeploys_`), and any `WorkerAssignmentManager` displaced-worker
  handlers are cleared before it or the base disappears.
- **Transfer (base) = identity-preserving ownership move, not snapshot recreate.**
  `Faction::TransferBaseTo` does `ReleaseBase` (moves the `unique_ptr<BaseManager>` out of the
  giver — same object, same address, same `baseId`, same `Tile`/`HomeBaseIndex`/
  `WorkerAssignmentManager`) → `BaseManager::RebindFaction` (repoints every per-faction
  dependency the base resolves through — effects provider, `ResearchManager` for tech-gated
  buildings/pop fallback, `EconomyManager` for the energy split — so the next resolve reads the
  new owner) → receiver `AddBase`. Deploy-cooldown records for that `baseId` migrate with the
  base (`Faction::MigrateBuildingDeploys_`) rather than leaking on the giver or vanishing on the
  receiver. The caller still recalculates composition/worker assignment afterward, since psych
  may differ under the new owner — but the `BaseManager*` itself never changes. The
  `HomeBaseIndex` moves with the object rather than the faction, so claims held by units the
  *receiver* owns stay valid; claims held by units of any other faction are **foreign** and are
  dropped on transfer. That is the same rule unit transfer applies, and it is load-bearing: a
  supply crawler homed at a base feeds that base's production
  (`ResourceManager::ComputeWorked_`), so leaving the loser's claims in place would have the
  captor harvesting the loser's crawlers.
- **Base introduction is auto-wired.** `Faction::AddBase` emits `OnBaseAdded` for every insertion
  — founding, post-transfer adopt, and (if it returns) future load — and `Engine` connects that
  once, at faction construction, to `EventBridge::WireBase`. `WireBase` is idempotent (tracks
  wired `BaseManager` objects — by address, not `baseId`, so a reconstructed base reusing its id
  is still wired), so it is safe to call from both the signal and any remaining explicit call
  site. No caller needs to remember to wire a captured/traded base by hand.
- **UI rule**: views may hold `BaseManager&` / `Unit*` for their whole life only while the
  protocol guarantees validity, or must subscribe to the signals above and pop/clear. Minimum
  bar: `BaseView` pops when its base is destroyed (`OnDestroyed`) or changes owner (its
  `GetFaction()` no longer matches the faction the view was opened for); `WorldView` clears
  `m_pSelectedUnit` on both `OnUnitDestroyed` and `OnUnitReleased` so selection can never dangle
  or silently point at a unit that just left the player's faction. See
  `docs/architecture/ui-system.md`, "Object Lifetime / Invalidation".
- **Out of scope here**: faction elimination / erasing a `Faction` mid-turn is still deferred —
  see `docs/architecture/turn-system.md`.

### Map System
- **Purpose**: Manages game world terrain and tile-based resource production
- **Components**:
  - `Tile`: A single map tile — position (x,y), terrain characteristics (Moisture_t, Rockiness_t, elevation, river/aquifer/fungus), and its improvements. Holds no worked-tile or ownership state; those live in the world-scoped indexes below.
  - `WorldMap`: Owns the tile grid plus `UnitPositionIndex`, `WorkedTileIndex` and `TerritoryMap`. Tile addresses are stable for its lifetime, so `GetTiles()` hands out a const-element span rather than the owning vector.
  - `ImprovementRegistry` / `ImprovementConfig_t`: One config type covering terrain classifications, natural features, player-built improvements, tile bonuses (`frequency` > 0) and landmarks. There is no separate tile-bonus registry.
  - `WorldGenerator`: Builds a `WorldMap` from a seed — elevation, moisture, rockiness, fungus, landmarks, aquifers/rivers, tile bonuses, in that order.
- **Dependencies**:
  - Faction subsystems (particularly Military with Bases) work tiles for resources
  - ImprovementRegistry loads from config/improvements.json
- **Details**: See `docs/architecture/map-system.md` for detailed architecture

### Unit Movement System
- **Purpose**: Tile entry costs, step legality, path planning, move-order execution, cargo transport, and base conquest
- **Components**:
  - `MoveCostCalculator`: Single home of the tile-entry rules — resolves a unit + tile into `EntryTerms_t` (fragment cost, fungus full-cost banking, forced end-of-turn) and a shroud-aware planning weight
  - `MovementRules`: Free functions for terrain-domain entry, unaided occupancy, full `CanEnterTile` (asks TransportRules only for boarding), ZOC, friendly occupant/base, and stacking
  - `TransportRules`: Cargo domains, capacity, load sites (`TransportParams`), boarding/unload helpers (`FindBoardableTransport`, `CanUnloadTo`, attach)
  - `AttackRules`: Attack legality — enterability, channel `Permission(Attack)`, targeting (`FindVisibleHostileOnTile`), declare gate (`FindAttackableHostileOnTile`)
  - `StepEvaluator`: Edge legality (adjacency, terrain domain, occupants, ZOC) at objective or faction-known knowledge levels
  - `Pathfinder`: Dijkstra over planned fragment costs and plannable steps
  - `UnitOrderExecutor`: Executes unit orders; spends fragments and banks multi-turn fungus charges per `EntryTerms_t`
  - `BaseConquestRules` / `BaseConquestEffects`: Pure conquest predicates (garrison, capture veto, species) split from the world mutations they gate (population loss, facility destruction, capture, raze, native raid)
  - `IUnitOrderWorld`: Narrow session surface — base lookup, intercept, conquest — that `GameState` implements and injects into `UnitOrderExecutor`; nullable so movement-only harnesses need no `GameState`
- **Dependencies**:
  - GameState owns UnitOrderExecutor and implements `IUnitOrderWorld` for it; all bind the live WorldMap
  - MoveCostCalculator reads ImprovementRegistry configs (move_cost / move_cost_override)
  - BaseConquestEffects reads `config/base_conquest.json` via `GameDataContext::baseConquestConfig`
- **Details**: See `docs/architecture/unit-movement-system.md` for detailed architecture

### UI System
- **Purpose**: View-stack management and layered rendering, with no backend dependency.
- **Build target**: `ac-ui`, a static library over the abstract `Graphics` / `Input` interfaces.
  It never links a rendering backend, so the test suite drives real views against a recording
  `Graphics`. The executable is `main.cpp` + `Engine.cpp` + the SFML backend.
- **Components**:
  - `UIManager`: one concrete class (there is no `SFMLUIManager` / `NullUIManager` — the backend
    split lives in `Graphics` / `Input`). Owns the overlay stack plus one persistent `IWorldView`.
  - `IGameView`: a screen or layer in the stack; `IWorldView` adds what the manager needs of the
    map view specifically.
  - `UIElement`: base for everything a view draws and hit-tests.
  - `ViewFactory`: builds views from `GameState` + `GameDataContext`; throws rather than
    returning a null view.
- **Factory**: `CreateUIManager()` returns the concrete manager (no compile-time flag).
- **Details**: See `docs/architecture/ui-system.md`.

### Configuration
- **Turn Stages Config**: `config/turn_stages.json` - The stage order, and the pre/post/replace hooks each stage carries (`HookContext`)
- **Improvements Config**: `config/improvements.json` - Loaded by ImprovementRegistry; defines terrain features, improvements, tile bonuses and landmarks, with their effects and `excludes` coexistence rules
- **World Gen Config**: `config/worldGen/` - `presets.json` (landmass recipes), `decoration.json` (moisture/rockiness/aquifer/fungus/bonus knobs), `landmarks.json` (placement recipes)
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
  - `CouncilOutcomeApplier`: Applies a passed proposal's outward mutations (energy grants) and Instantaneous governor effects (infiltration); world-parameter outcomes are deferred to `WorldEvents`.
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
