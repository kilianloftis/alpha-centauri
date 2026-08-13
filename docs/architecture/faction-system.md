# Faction System Architecture

```mermaid
graph TB
    subgraph "Session (GameState)"
        GameState[GameState<br/>owns m_factions]
        TurnProcessor[TurnProcessor<br/>Advance / StageResult_t]
        DiplomacyLedger[DiplomacyLedger<br/>world-scoped pairwise status]
        DiplomaticActionExecutor[DiplomaticActionExecutor]
        PlanetaryCouncil[PlanetaryCouncil]
        WorldMap[WorldMap]
        EventBus[EventBus<br/>mod-facing]
    end

    subgraph "Faction"
        Faction[Faction<br/>owns every box below]
        FactionIdentity[FactionIdentity]
        AIProfile[AIProfile]
        FactionFlavor[FactionFlavor<br/>base names, seeded]
        EconomyManager[EconomyManager<br/>treasury + econ/labs/psych split]
        Military[Military<br/>unit designs only]
        ResearchManager[ResearchManager]
        ResearchSelector[ResearchSelector]
        SocialEngineeringManager[SocialEngineeringManager]
        UnitManager[UnitManager<br/>owns Unit]
        Bases[m_bases<br/>vector of BaseManager]
        FactionEffectsPool[FactionEffectsPool]
        FactionExploredMap[FactionExploredMap]
        FactionVisibleMap[FactionVisibleMap]
        FactionRevealedUnits[FactionRevealedUnits]
    end

    subgraph "Base (BaseManager owns each)"
        BaseManager[BaseManager]
        PopulationManager[PopulationManager]
        PopContainer[PopContainer<br/>owns Pop]
        WorkerAssignmentManager[WorkerAssignmentManager]
        BuildingManager[BuildingManager]
        ResourceManager[ResourceManager]
        ProductionManager[ProductionManager]
        HomeBaseIndex[HomeBaseIndex<br/>supported units]
    end

    subgraph "Shared config (GameDataContext)"
        TechRegistry[TechRegistry]
        BuildingRegistry[BuildingRegistry]
        SocialPolicyRegistry[SocialPolicyRegistry]
    end

    GameState --> Faction
    GameState --> DiplomacyLedger
    GameState --> DiplomaticActionExecutor
    GameState --> PlanetaryCouncil
    GameState --> WorldMap
    GameState --> EventBus
    TurnProcessor -->|stages read| GameState

    Faction --> FactionIdentity
    Faction --> AIProfile
    Faction --> FactionFlavor
    Faction --> EconomyManager
    Faction --> Military
    Faction --> ResearchManager
    Faction --> ResearchSelector
    Faction --> SocialEngineeringManager
    Faction --> UnitManager
    Faction --> Bases
    Faction --> FactionEffectsPool
    Faction --> FactionExploredMap
    Faction --> FactionVisibleMap
    Faction --> FactionRevealedUnits

    Bases --> BaseManager
    BaseManager --> PopulationManager
    BaseManager --> WorkerAssignmentManager
    BaseManager --> BuildingManager
    BaseManager --> ResourceManager
    BaseManager --> ProductionManager
    BaseManager --> HomeBaseIndex
    PopulationManager --> PopContainer
    WorkerAssignmentManager -->|claims tiles in| WorldMap
    ResourceManager -->|reads split from| EconomyManager

    ResearchManager -->|borrows| TechRegistry
    BuildingManager -->|borrows| BuildingRegistry
    SocialEngineeringManager -->|borrows| SocialPolicyRegistry

    DiplomaticActionExecutor -->|moves energy/tech/bases between| Faction

    style Faction fill:#f9f,stroke:#333,stroke-width:4px
    style GameState fill:#fbf,stroke:#333,stroke-width:3px
    style BaseManager fill:#bfb,stroke:#333,stroke-width:2px
    style DiplomacyLedger fill:#fbf,stroke:#333,stroke-width:2px
    style TechRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style BuildingRegistry fill:#ffd,stroke:#333,stroke-width:2px
    style SocialPolicyRegistry fill:#ffd,stroke:#333,stroke-width:2px
```

## Component Overview

### FactionVector
- **Purpose**: Stores all Faction instances in the game
- **Implementation**: `std::vector<std::unique_ptr<Faction>>` owned by GameState
- **Responsibilities**:
  - Store all factions
  - Provide indexed access
- **Interaction**: Owned by GameState, accessed by TurnProcessor

### Faction construction
There is no `FactionFactory`. `Engine::StartNewGame_` constructs each `Faction` directly from
its `FactionConfig_t` (out of `GameDataContext::factionRegistry`) and hands it to
`GameState::AddFaction`.

A `Faction` is **valid when its constructor returns**. It takes, as required arguments:
`FactionId_t`, `bIsPlayerControlled`, `const FactionConfig_t&`, `const GameDataContext&`,
`WorldMap&`, `const GameSettings&`, and a `uint32_t seed`. The constructor sizes the
explored/visible maps from the world map and takes a first visibility reading.

- The **world map and settings** are constructor dependencies rather than post-construction
  `Bind`/`Set` calls: a faction without a map used to make `RebuildVisibility` a silent no-op,
  so a base founded on a not-yet-wired faction produced no visibility, no territory rebuild and
  no first-contact check while every getter still returned plausible values.
- **Visibility** is rebuilt from scratch whenever a vision source moves: every unit, every base,
  and every tile carrying a vision-granting improvement. Because the map is a constructor
  dependency, no live faction can have an unsized visible map, and `RebuildFromSources` throws
  rather than treating "unsized" as "sees everything".
  - The per-improvement sight radius is resolved **once at config load**
    (`ImprovementConfig_t::visionRadius`), not per tile per rebuild — it derives only from the
    improvement's own `ThisTile` `Vision` modifiers, which cannot change after load.
  - `Faction::DeferVisibilityRebuild()` returns an RAII scope that coalesces rebuilds; the
    outermost scope performs one. `UnitManager::DestroyUnit` holds one across its cargo
    recursion, so sinking a loaded transport rebuilds once rather than once per hull. A rebuild
    also drives a first-contact sweep over every other faction, which is what makes the
    per-event cost compound.
  - Still O(width × height) per rebuild for the improvement sweep: a world-level index of
    vision tiles needs a mutation choke point that `Tile` does not currently have.

- The **seed** drives every per-faction random choice (base names, the starting research
  target). It is injected rather than drawn from `std::random_device` inside `FactionFlavor` /
  `ResearchSelector`, because those choices are save-game state; `Engine` resolves one session
  seed and derives a sub-stream per faction.

`GameState::AddFaction` is **registration, not construction**. It pushes the faction into
`m_factions` *first*, then calls `AttachToSession_` to install the session back-pointer and the
observers (which necessarily close over `GameState`), and finishes with a territory/visibility
catch-up sweep plus a full first-contact pass. The order is load-bearing: the observers iterate
`Factions()`, so wiring before the push meant a faction arriving with bases already on the map
— load-game, or any future runtime creation — was never scanned.

Caller contract: the faction must have been constructed against the session's `WorldMap`.

### Faction
- **Purpose**: Represents a single faction in the game (player or AI)
- **Identity**: `FactionId` is minted by `GameState::AllocateFactionId()` (the sole allocator for
  this ID namespace); it and `bIsPlayerControlled` are required constructor
  arguments so a `Faction` always knows its own identity — it is never assigned after the fact.
  `Faction::CreateBase` threads `m_factionId` into each `BaseManager` it creates, so callers no
  longer pass a faction ID in from outside. `bIsPlayerControlled` is what
  `GameState::GetPlayerFaction()` searches for (not an index-0 convention); not used for
  multiplayer yet, but generalizes to more than one human-controlled faction without a
  representation change.
- **Responsibilities**:
  - Coordinate all faction subsystems
  - Provide unified interface for faction operations
  - Manage faction-specific state
- **Composition**: owns `FactionIdentity`, `AIProfile`, `FactionFlavor`, `EconomyManager`,
  `Military`, `ResearchManager`, `ResearchSelector`, `SocialEngineeringManager`, `UnitManager`,
  its `m_bases`, its effects pool, and its explored/visible/revealed-unit maps. **Not**
  diplomacy — that is world-scoped (see below).
- **Turn processing is not here.** `Faction` has no `ProcessTurn`; stages drive the turn.
- **Signals**: exactly one — `OnBaseAdded(BaseManager&)`, which `EventBridge` uses to wire each
  base and to publish `EvBaseBuilt`. Tech discovery is `ResearchManager::OnTechDiscovered`.
  There is **no** elimination signal: factions are never removed from the game (a defeated
  faction's leader can be freed to re-establish it), so there is nothing to observe. See
  `docs/game-rules-decisions.md`.

### FactionIdentity
- **Purpose**: Static faction information (name, leader, visual identity)
- **Responsibilities**:
  - Store faction name and leader name
  - Manage faction color for UI rendering
  - Reference faction logo texture
- **Rationale**: Separated to allow easy faction customization and modding

### AIProfile
- **Purpose**: Defines AI behavior and decision-making parameters
- **Responsibilities**:
  - Store personality traits (aggressive, peaceful, etc.)
  - Define priority weights for different game aspects
  - Provide behavior modifiers for AI decisions
- **Rationale**: Separated to allow different AI personalities and easy AI tuning

### Economy
- **Purpose**: Owns the faction energy treasury and the faction-wide econ/labs/psych split.
- **Responsibilities**:
  - Hold the treasury. `AddEnergy` is income; `SpendEnergy` / `CanAfford` are the spend path and
    enforce "never negative" here rather than in each caller.
  - Own the `EnergyAllocation_t` percentages every base's `ResourceManager` splits against.
- **Not here**: minerals and nutrients are per-base stockpiles (`ProductionManager`,
  `PopulationManager`); there are no credits distinct from energy, and no trade routes.
- **Implementation**: See `docs/architecture/economy-system.md`.

### Military
- **Purpose**: Holds the faction's **unit designs** and the **built-component ledger** backing
  prototypes.
- **Prototype ledger**: `RecordBuiltComponents` marks every filled component of a design as
  fielded; `UnitManager::CreateUnit` calls it for *every* unit the faction gains, so starting
  units, produced units and conquest spawns all share one ledger. `IsPrototype(design)` is true
  while any filled component is still unfielded — several unknown components on one design are
  still a single prototype.
- **Two consumers, one question**: `BaseManager::IsCurrentProductionPrototype_` asks it for the
  mineral surcharge (`production.json` `prototype_surcharge_percent`), and `Unit`'s constructor
  asks it once and latches the answer into `Unit::IsPrototype()`, which the `IsPrototype`
  `UnitFilter_t` reads. The latch is what keeps the filter a context-free identity predicate:
  the ledger keeps moving as the faction builds, but a unit's prototype status is fixed at
  construction.
- **Not here**: live units belong to `UnitManager` (which creates and destroys them and owns the
  `Unit` objects); bases belong to `Faction::m_bases`. There is no `UnitFactory`.

### ResearchManager
- **Purpose**: Manages faction's technological progress
- **Responsibilities**:
  - Track discovered technologies (vector<TechId>)
  - Manage current research target (TechId)
  - Track accumulated research points
  - Calculate points needed for current tech via TechCostCalculator
  - Reference TechRegistry for tech definitions
- **Composition**: Uses TechCostCalculator, references TechRegistry
- **Rationale**: Research system is complex with its own data structures

### Diplomacy — world-scoped, not a faction subsystem
`Faction` owns no diplomacy object. Pairwise status lives in `DiplomacyLedger` on `GameState`,
because a relationship is a property of the *pair*, not of either side; storing it per faction
would mean two copies that can disagree. Proposals and trades run through
`DiplomaticActionExecutor`, also on `GameState`.

See `docs/architecture/diplomacy-system.md`.

### Base System
- **Purpose**: Represents individual bases that provide resources for a faction
- **Components**:
  - `BaseManager`: the base. (Older docs called this `Base`; there is no separate `Base` type.)
  - `PopulationManager`: API surface for the population component; manages growth and riot state for a single base, and **owns population policy** — which types a pop may become (`ResolveType_` walks the obsolescence chain against discovered techs and is the single place a requested type becomes an actual one, so `AddPop`, `ConvertTo`, `ConvertToFallback` and composition reconciliation all apply the same rule) and how composition targets are reconciled (`ApplyCompositionTargets`). The rules deliberately do not live in `PopContainer`: when they did, the tech gate was applied on one conversion path and not another.
  - `IConstructable`: Abstract interface for any entity that can be queued for production; exposes `GetId()`, `GetName()`, and `GetMineralCost()`
  - `ProductionManager`: API surface for the production component; manages one active `IConstructable` at a time, tracks accumulated minerals, and emits `OnProductionCompleted` when the item is finished
  - `WorkerAssignmentManager`: Owns the set of workable tiles and the tile-scoring policy; holds a reference to the base's `PopulationManager` and to the world-scoped `WorkedTileIndex`; validates worker-to-tile assignments and runs auto-assignment. An assignment is a `WorkedTileClaim` minted by `WorkedTileIndex::TryClaim` and held by the `Pop`; the claim also carries the user-assigned flag, so the manager can skip user-assigned pops during auto-assignment and the flag can never outlive the assignment (see `docs/architecture/map-system.md`, "WorkedTileIndex").
  - **Pop roles** (`PopRole_t`, config key `role`, required): `worker`, `drone`, `talent`, `specialist`. A closed partition — every pop is exactly one — so `IsPlainWorker()` / `IsDrone()` / `IsTalent()` / `IsSpecialist()` cannot overlap. Role is declared, not inferred: it used to be read off `riot_contribution` / `golden_age_contribution` magnitudes, which made role and magnitude the same knob and left a non-worker with a golden-age contribution counting as both specialist and talent. `PopTypeRegistry` rejects `role: specialist` with `can_work_tile: true` and vice versa, so `IsWorker()` (the capability) and `IsSpecialist()` (the role) stay consistent.
  - **Obsolescence** (`obsoletes`): transitive and closed over the whole graph, *not* gated on intermediate steps' techs — if Transcend obsoletes Empath and Empath obsoletes Doctor, then Transcend obsoletes Doctor even in a tech order that skipped Empath. `PopTypeAvailabilityCalculator::GetAvailable` and `ResolveCurrentType` are both phrased in terms of one walk, so they cannot disagree; a type is assignable exactly when it is `player_assignable`, its tech is discovered, and `ResolveCurrentType` returns the type itself. Cycles are rejected by `PopTypeRegistry` at load, since the walk is on the per-turn path for every pop of every base.
  - `PopContainer`: Storage for a single base's `Pop` instances — the vector, the counts, and the revision. Owns no population policy: `AddPop` and `ConvertTo` take an already-resolved `PopTypeConfig_t`, because deciding which types are legal is `PopulationManager`'s job. Note `GetWorkerCount()` is every tile-capable pop (plain workers *and* drones *and* talents) and so does not partition the population against `GetDroneCount`/`GetTalentCount`/`GetSpecialistCount`; `GetPlainWorkerCount()` is the disjoint bucket.
  - `Pop`: Individual population unit; holds a `WorkedTileClaim` when assigned as a worker (`GetTile()` reads it), which releases the tile automatically when the pop dies, converts to a non-worker type, or is reassigned
  - `RiotCalculator`: Tracks drone riot state and emits `will_riot`, `is_rioting`, and `riot_ended`. Two sources keep a riot alive and they expire differently: the **natural condition** (drones exceed the composition's talent target) is recomputed on every `Update`, while a **forced riot** (probe Incite Drone Riots) lasts a fixed number of end-of-turn passes. A forced riot needs its own lifetime precisely because the action does not change composition — the natural condition cannot sustain it.
  - `GrowthCalculator`: Computes the nutrient threshold required for a base to grow one population, from `GrowthConfig_t`. Growth/starvation decisions (stockpile ≥ required → grow; stockpile < 0 → starve) are made in the `Population` turn stage.
  - `GrowthConfigParser`: Loads `config/pop_growth.json` and produces a `GrowthConfig_t` holding `nutrients_per_pop` and `max_base_size`. Both keys are required and must be positive integers — a `nutrients_per_pop` of 0 makes the growth threshold identically 0, so every base would grow every turn.
  - `SecretProjectAvailabilityCalculator`: Answers two distinct questions about a secret project, which used to be conflated into one. `IsUnavailable` — nobody may build it, because some faction holds it **or** the copy was destroyed (tombstoned via `GameState::MarkSecretProjectDestroyed`). `IsOwnedByAnyFaction` — some faction holds it right now, which is what a UI label, a victory check or diplomacy wants; a razed project answers `false`. Injected into `BuildingManager`; `CanAddBuilding` consults it at the point a building is granted, not only where the build menu is drawn
  - `Buildings`: Collection of building IDs in the base
  - `TileResources`: Resources (nutrients, energy, minerals) from worked tiles
  - `Position`: Map coordinates (x, y) used to calculate the workable tile radius
- **Responsibilities**:
  - Manage population growth and size (1-8 initially, expandable with buildings)
  - Expose the set of workable tiles via `WorkerAssignmentManager::GetWorkableTiles()` (5×5 grid minus corners, Manhattan distance ≤ 3 within [-2,2] offsets, 20 tiles, excluding own tile). Tiles already worked by another base — including another faction's — cannot be worked (enforced by `WorkedTileIndex`); enemy-unit blocking is a TODO pending unit implementation.
  - Hold the worker-to-tile assignment on each `Pop` as a `WorkedTileClaim` (read via `Pop::GetTile()`)
  - Let `WorkerAssignmentManager` validate tiles against the workable set and auto-assign idle workers; one-worker-per-tile uniqueness is enforced structurally by `WorkedTileIndex::TryClaim`
  - Assign workers to different roles (tiles, labs, psych, econ, drones, talents)
  - Track buildings constructed in the base
  - Manage one active production item per base, accumulating minerals each turn until the item's mineral cost is paid; emits `OnProductionCompleted` and adds the building to the base when finished
  - Calculate resource output based on worker assignments:
    - Workers/Talents work tiles (produce nutrients, energy, minerals)
    - Lab workers contribute to research
    - Psych workers contribute to psych
    - Econ workers produce energy directly
    - Drones produce nothing
  - Manage trade routes for additional energy
  - Provide resource calculation methods (CalculateNutrients_, CalculateEnergyProduction_, CalculateMinerals_)
  - Evaluate drone riot conditions and manage riot lifecycle via signals:
    - `will_riot`: emitted during `AddPop()` when the new composition triggers drone riot but the base is not yet rioting
    - `is_rioting`: emitted at end of turn (`CheckRiotEndOfTurn`) while the base is rioting from **either** source — the natural condition or an unexpired forced riot — and immediately by `ForceRiot` when it starts one
    - `riot_ended`: emitted at end of turn when neither source holds any longer and the base was previously rioting
- **Rationale**: Bases are the primary source of resources and require complex management of population with specialized worker roles, buildings, and tile resources
- **Destroy vs. transfer**: a `BaseManager` is destroyed only by raze/extract (`Faction::ExtractBase`); ownership change (capture, mind-control, trade) is an identity-preserving move (`Faction::TransferBaseTo`) that rebinds the same object to a new `Faction` rather than recreating it. See `docs/architecture/high-level.md`, "Object lifetime and ownership transfer", for the full protocol, including how `HomeBaseIndex`, deploy cooldowns, and `EventBridge` wiring survive each case.

## Integration with Engine

### Turn Processing Flow
Turns are **stage driven**, not faction driven. `Faction` has no `ProcessTurn`.

1. `TurnProcessor::Advance(GameState&)` walks the stage list from `config/turn_stages.json`.
2. A global stage runs once; a per-faction stage runs once per faction in `GameState::Factions()`.
3. Each stage reads what it needs off `GameState` and the faction it was handed, and returns a
   `StageResult_t` — `Continue` to move on, or `Yield` to hand control back to the UI and resume
   at the same stage on the next `Advance` (this is how `PlayerActions` waits for the player).
4. Stages self-register, so adding one is a new translation unit plus a config entry.

See `docs/architecture/turn-system.md` for the stage list and the resume protocol.

### Engine Ownership
- Engine owns GameState
- GameState owns FactionVector (vector of unique_ptr<Faction>)
- Each Faction owns its subsystems
- `Engine::StartNewGame_` constructs factions and registers them via `GameState::AddFaction`
- This hierarchy ensures proper lifetime management

### Modding Integration
- Faction definitions load from config into `GameDataContext::factionRegistry`; `FactionIdentity`
  and `AIProfile` are built from that config.
- Techs, buildings, unit components, social policies and improvements are all registry-loaded
  config; see `docs/architecture/high-level.md`, "Configuration".
- `HookContext` (not a "HookSystem") lets a turn stage carry pre / post / replace hooks; see
  `docs/architecture/event-system.md` for that seam and for the mod-facing `EventBus`.

## Design Rationale

### Separation of Concerns
- Each subsystem has a single, well-defined responsibility
- Follows Single Responsibility Principle
- Makes testing and maintenance easier

### Moddability
- Static data (FactionIdentity, AIProfile) separated from dynamic state
- Easy to add new factions via configuration
- AI profiles can be customized without code changes

### Extensibility
- New subsystems can be added to Faction without affecting existing code
- Open/Closed Principle: open for extension, closed for modification
- Interface-based design allows different implementations

### Performance Considerations
- `GameState::FindFaction` is a linear scan over `m_factions`. Faction counts are single digits,
  so this has never been the cost that mattered; the recomputation the reviews keep finding
  (per-frame panel work, per-event visibility rebuilds) is.
- Derived state is memoized against `Revision` counters rather than recomputed: the faction
  effect pool, `BaseManager`'s composed base effects, the social-rating map, and the UI's
  per-panel snapshots all follow the same pull-based invalidation pattern.
