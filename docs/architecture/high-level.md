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
        BasePanels[BaseDisplay, BaseWorkableAreaDisplay,<br/>PopulationDisplay, GrowthDisplay, CommerceDisplay<br/>implement IBasePanel]
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
        NativeUnitRegistry[NativeUnitRegistry]
        PopCompositionConfig_t[PopCompositionConfig_t]
        PopCompositionCalculator[PopCompositionCalculator]
        HurryProductionCalculator[HurryProductionCalculator]
        ScrapRefundCalculator[ScrapRefundCalculator]
        CommerceConfig_t[CommerceConfig_t]
        ScrapPayout[ScrapPayout<br/>plan + credit]
        LuaRuntime[LuaRuntime]
    end

    subgraph "Faction System"
        GameState[GameState<br/>(mutable save-game data)]
        WorldMap[WorldMap]
        FactionVector[FactionVector<br/>vector&lt;unique_ptr&lt;Faction&gt;&gt;]
        Faction[Faction]
        FactionSubsystems[Faction Subsystems:<br/>FactionIdentity, AIProfile,<br/>Economy, Military,<br/>Research, SocialEngineering]
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

    subgraph "Ecology System"
        EcologyLedger[EcologyLedger<br/>blooms / grants / virtual minerals]
        BaseEcology[BaseEcology<br/>per-base score + memo]
        EcoDamageCalculator[EcoDamageCalculator<br/>eco_damage.lua]
        WorldEventTracker[WorldEventTracker<br/>active world events]
    end

    subgraph "Configuration"
        TurnStagesConfig[config/turn_stages.json]
        ImprovementsConfigFile[config/improvements.json]
        DifficultyConfigFile[config/difficulty.json]
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
    GameState -.->|reads| GameDataContext
    DifficultyConfigFile --> GameDataContext
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
    GameDataContext --> CommerceConfig_t
    ScrapRefundCalculator --> LuaRuntime
    ScrapPayout -->|consumes quote| ScrapRefundCalculator
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

    GameState --> EcologyLedger
    GameState --> WorldEventTracker
    BaseEcology --> EcoDamageCalculator
    BaseEcology --> EcologyLedger

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
- **Who holds it**: `Engine` owns the context and holds it `const`. `GameState`, `Faction` and `ViewFactory` receive `const GameDataContext&` at construction, and `GameState::GetGameData()` is the only accessor that hands it back out.
  - Free rule functions and turn stages read it from the `GameState&` they already take. They never reach it through a faction, base or unit.
  - The classes those owners build take the pieces they use as constructor references. `MoraleCalculator` (a stateless view over `moraleConfig`, owned here beside `TechCostCalculator`) reaches `Unit` via `Faction` → `UnitManager` without appearing in any create/transfer signature — `UnitManager::CreateUnit` and `Faction::TransferUnitTo` take no morale argument.
  - A class that needs many pieces takes a named bundle of references: `BaseManager` takes `BaseRules_t` (`include/game/faction/base/BaseRules.h`), which `MakeBaseRules` builds from the context and which the constructor unpacks without keeping.
  - Only the loader, the validators and `MakeBaseRules` take the context as a function parameter.
- **Not changed after loading**: rules for one world live on its `WorldMap`, not here. The world's elevation rules carry the preset's range; the context keeps `map_rules.json` as parsed.
- **Outlives all live state**: `GameState`, every `Faction`, `BaseManager`, and `TileEffectsContext` hold non-owning references into this object, so `Engine` declares `m_gameDataContext` *before* `m_pGameState`. Members are destroyed in reverse declaration order, so the whole faction/base/unit graph is torn down while the definition data is still alive.
- **Components**:
  - `IConstructable`: Abstract interface for entities that can be constructed in a base; exposes `GetId()`, `GetName()`, and `GetMineralCost()`
  - `BuildingRegistry`: All building definitions loaded from `config/buildings.json`; each entry may have `secret_project: true` to mark it as a Secret Project
  - `StockpileRegistry`: Never-completing production items loaded from `config/stockpiles.json` (see `config/README-stockpiles.md`). Queued like a building but never constructed: each turn `BaseProduction` converts the base's leftover minerals through the queued entry's `MineralsConverted` modifiers. Also supplies the empty-queue default via `FindFallback`
  - `TechRegistry`: All tech definitions loaded from `config/techs.json`
  - `PopTypeRegistry`: All pop type definitions loaded from `config/pop_types.json`
  - `PopCompositionConfig_t`: Composition formula config loaded via Lua
  - `PopCompositionCalculator`: Evaluates composition formulas at runtime
  - `HurryProductionCalculator`: Prices energy-for-minerals hurrying from `production.json` `kinds.<kind>.hurry`; borrowed by every `BaseManager`
  - `ScrapRefundCalculator`: Prices player scrap from `production.json` `kinds.<kind>.default_scrap`; unit/building configs may override formula and refund_type (`StatId_t` whitelist), or set `"formula": null` to deny scrap. Borrowed by `BaseManager` (buildings) and `Faction` (units). Where the refund *lands* is `ScrapPayout`'s job, not the calculator's
  - `CommerceManager` / `CommerceCalculator`: faction-owned commerce queries; calculator pairs Treaty/Pact bases and feeds commerce into `ResourceManager` raw energy (see [economy-system.md](economy-system.md))
  - `DifficultyConfig_t`: Session difficulty levels loaded from `config/difficulty.json`. Each level carries an `effects` list (injected per faction into `FactionEffectsPool`) and a `DifficultyRules_t` of non-effect knobs. `GameRulesConfig_t::difficultyId` selects one; empty defers to the file's `default`. See [difficulty-system.md](difficulty-system.md)
  - `LuaRuntime`: Shared Lua state used to load and evaluate config scripts
- **Note**: Implemented as a plain struct with public `unique_ptr` members (no getters/setters needed)
- **Valid by construction**: `LoadGameData(paths)` is a *factory* — it returns a fully populated
  context by value (the struct is move-only) rather than filling a default-constructed bag.
  `ThrowIfIncomplete` runs before it returns and throws naming the first null member, so a
  partially-loaded context cannot escape the loader. **Consumers may therefore dereference any
  member without checking**, and the subsystems that need pieces of it take them as constructor
  references rather than nullable pointers. Test fixtures deliberately assemble a narrower
  context from `tests/fixtures/` and do not run the completeness check; the reference-typed
  constructors are what stop them building a half-valid object from it. `FixtureDataPaths()`
  gives `LoadGameData` a complete fixture set. Tests never read `config/`: the shipped config is
  checked when the game loads it at startup.

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
  - `GameState`: Owns FactionVector, missionYear, and WorldMap — mutable data written to and read from disk. Also owns two world-scoped resolvers that must share the map's lifetime rather than GameDataContext's: `TileEffectsContext` (bundles the live WorldMap with the immutable ImprovementRegistry to resolve tile effects) and the stateless `UnitOrderExecutor`. `SecretProjectAvailabilityCalculator` lives here too, since it scans the live faction vector — as an owned member of the object it queries, it cannot dangle the way a `GameDataContext`-owned reference into it could. `GameState` is also the sole owner of faction/base ID allocation, via two `IdAllocator` (`lib/IdAllocator.h`) members — the only place either ID namespace is minted, so any future runtime faction/base creation (not just Engine's composition root) has somewhere to get a unique ID from. `GetPlayerFaction()` returns whichever `Faction` has `IsPlayerControlled() == true` (set at construction), not an index-0 convention — see the `Faction` bullet below. `GameState` holds the ruleset by reference and hands it to session-level code through `GetGameData()`; the Planetary Council and world events read their configs from it — see the `GameDataContext` section above.
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

### Population System
- **Purpose**: A base's pops — how many, what type each is, and whether the base riots or enters
  a golden age.
- **Components**:
  - `PopContainer`: storage for the pops, the counts, and the revision. No policy.
  - `PopulationManager`: population policy — growth, pop loss, conversion legality, and the
    composition orchestration.
  - `DroneCalculator`: the three drone-pressure terms that need real math (bureaucracy residue,
    size-free subtraction, occupation decay). Each emits a contribution; there is no combined
    drone formula.
  - `PopCompositionCalculator`: phase 1 — the psych ladder, drone/talent annihilation, and
    lightest-type-first seating. Works on counts only.
  - `MoodLatch` + `RiotCalculator` / `GoldenAgeCalculator`: both moods are a sum of per-pop
    weights against a threshold, over the composition pool rather than base size, and both run
    the same forecast (Population stage) / commit (Mood stage) lifecycle.
  - `BaseMoodEffects`: the one place a riot tier's effect array is split between the base lane
    (`BaseEffectsCache`) and the faction lane (`FactionEffectsPool`), so a `FactionUnits`
    penalty declared by a rioting base actually reaches its units.
  - `BuildingDestruction` / `RebelFactionPicker`: shared consequences of riot escalation, also
    used by conquest and probe sabotage.
  - `PopTypeRegistry`: derives each type's `PopClass_t` from the promotion graph at load, and
    rejects a graph that is not a single chain through `is_default`.
- **Dependencies**:
  - `BaseManager` supplies everything composition needs from the effect list (drone pressure,
    talents, psych, both thresholds) — only it can build the base's effect list
  - `StatId_t::Drones` is drone *pressure*, resolved like any other stat; `drone_weight` is how
    much of it one body absorbs, and is distinct from `riot_weight`
- **Details**: See `docs/architecture/population-system.md`. **Phase 2** (reconciling computed
  counts against actual pops) is not implemented — composition computes but changes no pop.

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
  reassigned. Permanent `ProducedAtThisBase` grants stamped at construction stay on the unit
  (`Unit::GetProductionGrants`); they do not re-query the new owner's pool by a production-base
  id, so capturing the old base cannot retroactively add train bonuses. Mods/observers see one
  adopt event, never a fake death-then-birth pair.
- **Destroy (base) = raze, then reap — two steps, deliberately.** A base leaves the game the
  instant its population hits zero, from `BaseManager`'s pop-loss handler, whatever emptied it
  (starvation, a production pop cost, genetic plague, conquest). That handler calls
  `Faction::RazeBase`, which does everything the rest of the game can observe: tombstones any
  secret project the base held, orphans every `HomeBaseIndex` claim into it (units keep existing,
  just lose that home), releases the base's tile improvement, drops deploy-cooldown records for
  that `baseId` (`Faction::DropBuildingDeploys_`), emits `OnDestroyed` (while the object is still
  fully valid, so an open `BaseView` can pop before the reference dangles), and marks the base
  razed. `Faction::Bases()` filters razed bases out and `FindBase` stops returning them, so every
  existing loop drops it at once with no call-site changes.
  The `BaseManager` object is destroyed separately, by `Faction::ReapRazedBases`, which
  `TurnProcessor::Advance` calls between turn stages. Splitting the two is what makes razing safe
  from inside one of the base's own signal handlers and from the middle of a `Bases()` loop —
  neither destroys anything under the caller. The reap point carries no game meaning; a razed
  base is already gone as far as anything can tell. `Faction::ExtractBase` remains the manual
  extract-and-destroy used by snapshot reconstruction and tests, not the raze path.
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
  - `Tile`: A single map tile — position (x,y), terrain characteristics (Moisture_t, Rockiness_t, elevation, river, aquifer), optional terrain occupants (fungus, landmarks, bonuses, Monolith), its improvements, and the coexistence waivers those improvements were built under. Holds no worked-tile or ownership state; those live in the world-scoped indexes below.
  - `WorldMap`: Owns the tile grid plus `UnitPositionIndex`, `WorkedTileIndex` and `TerritoryMap`. Tile addresses are stable for its lifetime, so `GetTiles()` hands out a const-element span rather than the owning vector.
  - `ImprovementRegistry`: every tile occupant, from both `improvements.json` and `terrain.json`, with `ImprovementConfig_t::placement` telling improvements from terrain features (rockiness, moisture, water bands, river, aquifer, fungus, landmarks, bonuses, Monolith). An improvement with a `turns_required` is a former project. `TerrainOperationRegistry` holds the closed terrain mutations.
  - `WorldGenerator`: Builds a `WorldMap` from a seed — elevation, moisture, rockiness, fungus, landmarks, aquifers/rivers, tile bonuses, in that order.
- **Dependencies**:
  - Faction subsystems (particularly Military with Bases) work tiles for resources
  - ImprovementRegistry loads config/improvements.json and config/terrain.json together (`LoadOccupants`); TerrainOperationRegistry loads config/terrain.json
- **Details**: See `docs/architecture/map-system.md` for detailed architecture

### Unit Movement System
- **Purpose**: Tile entry costs, step legality, path planning, move-order execution, cargo transport, and base conquest
- **Components**:
  - `MoveCostCalculator`: Single home of the tile-entry rules — resolves a unit + tile into `EntryTerms_t` (fragment cost, fungus full-cost banking, forced end-of-turn) and a shroud-aware planning weight
  - `MovementRules`: Free functions for enter-grid terrain, unaided occupancy, full `CanEnterTile` (asks TransportRules only for boarding), ZOC grid, friendly occupant/base, and stacking
  - `TransportRules`: Cargo domains, capacity, load sites (`TransportParams`), boarding/unload helpers (`FindBoardableTransport`, `CanUnloadTo`, attach)
  - `AttackRules`: Attack legality — enterability (`CanAttackTile` = enter), targeting (`FindVisibleHostileOnTile`), declare gate (`FindAttackableHostileOnTile` + `attack_unit` resolve). Bombard legality is `IsWithinBombardRange` (any fragments, Chebyshev `bombard_range`, no visibility or entry requirement). `CollectBombardTargets` chooses a duel or per-unit strikes from the shared hostile census. `TileHasUnits` and `NonBaseImprovementIds` are the empty-tile wreck query
  - `StepEvaluator`: Edge legality (adjacency, terrain domain, occupants, ZOC) at objective or faction-known knowledge levels
  - `Pathfinder`: Dijkstra over planned fragment costs and plannable steps
  - `UnitOrderExecutor`: Executes unit orders; spends fragments and banks multi-turn fungus charges per `EntryTerms_t`
  - `TerritoryEntryEffects`: `BreakAgreementAndContinue`, the player's choice to break the agreement when a move order stops at territory its faction may not enter — declare Vendetta on the owner and resume the order
  - `BaseConquestRules` / `BaseConquestEffects`: Pure conquest predicates (garrison, capture veto, species) split from the world mutations they gate (population loss, facility destruction, capture, raze, native raid)
  - `IUnitOrderWorld`: Narrow session surface — base lookup, intercept, conquest — that `GameState` implements and injects into `UnitOrderExecutor`; nullable so movement-only harnesses need no `GameState`
- **Dependencies**:
  - GameState owns UnitOrderExecutor and implements `IUnitOrderWorld` for it; all bind the live WorldMap
  - MoveCostCalculator reads ImprovementRegistry configs (`move_cost`, and `move_cost` MaxClamp effects)
  - BaseConquestEffects reads `config/base_conquest.json` via `GameDataContext::baseConquestConfig`
- **Details**: See `docs/architecture/unit-movement-system.md` for detailed architecture.
  Native life designs: `docs/architecture/native-units-system.md`.

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
- **Improvements Config**: `config/improvements.json` defines improvements, each with its own build time and cost when a former can build it. `config/terrain.json` defines terrain occupants (`features`) and the closed terrain operations (`operations`).
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
  - `PlanetaryCouncil`: The vote lifecycle (propose → vote/veto → resolve), membership (seated at construction, `Expel` the one way out), governorship, and active-proposal set. Owned by `GameState` (`std::unique_ptr`); exposes `OnProposalOpened` / `OnResolved` signals for the UI.
  - `CouncilEffects`: Store for the continuous `ActiveEffect_t`s the council projects — world-global effects from proposals in force, plus the governor's faction-global effects.
  - `CouncilOutcomeApplier`: Applies a passed proposal's `on_passed_effects` (energy grants) and a new governor's `on_elected_effects` (infiltration); world-parameter outcomes are deferred to `WorldEvents`.
  - `CouncilProposalRegistry`: Proposal definitions loaded/validated from `config/council/`.
- **Dependencies**:
  - `GameState` owns the council and folds `CouncilEffects` output into the faction effect pool
  - Reads `Faction` population/effects, `DiplomacyLedger` (commlinks/infiltration), and `ResearchManager` (tech gating)
- **Details**: See `docs/architecture/council-system.md` for detailed architecture

### Atrocity System
- **Purpose**: The penalty layer behind the U.N. Charter — what a forbidden act costs its perpetrator, and the record of who has committed what.
- **Components**:
  - `AtrocityLedger`: Append-only record of committed atrocities plus the standing commerce-sanction expiry per faction. World-scoped, owned by `GameState` (`std::unique_ptr`), sibling of `DiplomacyLedger`.
  - `AtrocityRules`: Pure decisions — effective severity after Simple-count escalation, whether penalties apply, sanction expiry year, and which faction a blast is answered to.
  - `AtrocityEffects` (`CommitAtrocity`): Records the commission, then charges a counted act — sanctions, universal Vendetta (living AI only, via `ApplyStatusChange`), council expulsion, player notice. Victim memory is the record.
  - `AtrocitiesConfig_t`: `config/atrocities.json`. Severities are the closed enum `Simple` and `Major`, each with its own consequences. A counted Simple act that would pass the session level's atrocity threshold is answered for as Major. Landing exactly on the threshold stays Simple. A Major act does not add to that counter and carries no commerce sanction. The Charter gates both tiers. Either party being a Progenitor excuses the act. The threshold itself lives in `difficulty.json`, which states `player_atrocity_threshold` and `ai_atrocity_threshold` per level.
- **Dependencies**:
  - `CommitAtrocity` is authored in trigger lists (`genetic_plague`'s `on_success_effects`; the Planet Buster `on_detonate_effects`). Planet Buster victim comes from `AtrocityRules::BlastVictim` over what `Explosion` destroyed (first foreign base, else first foreign unit), handed over in `derivedVictim` rather than territory
  - Writes Vendetta (through `ApplyStatusChange`) and `PlanetaryCouncil::Expel`
  - Read by `CommerceCalculator` (sanctions zero a pair) and `BaseEcology` (counted records' `eco_virtual_minerals`)
  - Unknown severity names fail when the effect list or `atrocities.json` is loaded
- **Details**: See `docs/architecture/atrocity-system.md` for detailed architecture

### Ecology System
- **Purpose**: SMAC's per-base ecological damage — a fungal-pop percentage each base rolls every turn — plus the world-event registry whose Perihelion scales it.
- **Components**:
  - `BaseEcology` (owned by `BaseManager`, read through `GetEcologicalDamage()`): assembles the terraform sum, minerals, the resolved eco stats and the session ledgers into `EcoDamageInputs_t`, memoized on every revision an input reads.
  - `EcoDamageCalculator`: thin Lua bridge over `config/eco_damage.lua`; owned by `GameDataContext`.
  - `EcologyLedger`: per-faction fungal blooms, clean-mineral grants and virtual minerals. World-scoped, owned by `GameState`, sibling of `AtrocityLedger` and `MindControlLedger`.
  - `WorldEventTracker`: the active set of `config/world_events.json` events. Created by `GameState::CreateWorldEvents`; advanced by the `WorldEvents` stage; active events' effects reach every faction through `CollectWorldExtras`.
  - `NativeLifeLevelConfig_t`: `config/native_life_levels.json`, selected by `GameRulesConfig_t::nativeLifeLevelId` and injected into `FactionEffectsPool` like difficulty.
- **Dependencies**:
  - The `EcoDamage` stage rolls the score after `WorldEvents`, and applies `eco_damage.json`'s `on_pop_effects` (`FungalBloom`) on a hit
  - `GrantCleanMinerals` (eco facilities' `on_complete_effects`) and `AddVirtualMinerals` (Tectonic Payload's `on_detonate_effects`) write the ledger
- **Details**: See `docs/architecture/ecology-system.md` for detailed architecture

### UI Components
- **Purpose**: Display components that render game information using the Graphics interface
- **Components**:
  - `IBasePanel`: Interface for panels coordinated by `BaseView`
  - `BaseDisplay`: Displays base name, resource stockpiles, and click status text
  - `PopulationDisplay`: Displays current population and per-pop type breakdown
  - `GrowthDisplay`: Displays nutrient stockpile, growth threshold, and nutrient production
  - `CommerceDisplay`: Lists Treaty/Pact commerce partners for the open base (shorthand, our energy, their energy)
  - `WorldDisplay`: Displays the world map as a grid of tiles with terrain info
  - `BaseWorkableAreaDisplay`: Displays the 21-tile workable area around a base with resource production
- **Dependencies**:
  - All UI components depend on Graphics for rendering
  - PopulationDisplay subscribes to EventBus for population change events
  - WorldDisplay reads from Tile objects for terrain data
  - `BaseView` coordinates `BaseDisplay`, `BaseWorkableAreaDisplay`, `PopulationDisplay`, `GrowthDisplay`, and `CommerceDisplay` via `IBasePanel`
- **Details**: See `docs/architecture/graphics-system.md` for detailed UI component documentation
