# Turn System Architecture

```mermaid
graph TB
    subgraph "Composition Root"
        Engine[Engine::InitializeUi_]
    end

    subgraph "Configuration"
        ConfigFile[config/turn_stages.json]
        TurnStageConfigParser[TurnStageConfigParser]
        TurnStageConfig[TurnStageConfig_t<br/>id, name, description<br/>bRepeatForEachFaction<br/>hookContext]
    end

    subgraph "Stage Construction"
        TurnStageFactory[TurnStageFactory]
        TurnStageRegistrar["TurnStageRegistrar&lt;T&gt;<br/>(compile-time Global vs PerFaction)"]
        GlobalCreators[Global creator registry]
        PerFactionCreators[Per-faction creator registry]
        CustomGlobalTurnStage[CustomGlobalTurnStage]
        CustomPerFactionTurnStage[CustomPerFactionTurnStage]
    end

    subgraph "Stage Interfaces"
        TurnStageBase[TurnStageBase<br/>OnEnter/OnExit + OnExitImpl]
        GlobalTurnStage["GlobalTurnStage<br/>Execute(GameState&amp;)"]
        PerFactionTurnStage["PerFactionTurnStage<br/>Execute(GameState&amp;, Faction&amp;)"]
    end

    subgraph "Built-in Global Stages"
        TurnStart[TurnStart]
        WorldEvents[WorldEvents]
        VictoryConditionChecks[VictoryConditionChecks]
        TurnEnd[TurnEnd]
        Save[Save]
    end

    subgraph "Built-in Per-Faction Stages"
        ResourceCollection[ResourceCollection]
        UnitSupport[UnitSupport]
        BaseProduction[BaseProduction]
        BaseGrowth[BaseGrowth]
        IncomeCollection[IncomeCollection]
        ResearchAccumulation[ResearchAccumulation]
        Upkeep[Upkeep]
        Population[Population]
        PlayerActions[PlayerActions]
        PostActionsProduction[PostActionsProduction]
        Mood[Mood]
    end

    subgraph "Turn Execution"
        TurnProcessor[TurnProcessor]
        Advance["Advance(GameState&amp;)"]
        GlobalRegistry["m_globalRegistry"]
        PerFactionRegistry["m_perFactionRegistry"]
        StageOrder["m_stageOrder"]
    end

    subgraph "Hook_t System"
        HookContext[HookContext]
        Hook_t[Hook_t<br/>modId, scriptPath, callback]
    end

    Engine --> TurnStageFactory
    Engine --> TurnProcessor

    ConfigFile --> TurnStageConfigParser
    TurnStageConfigParser --> TurnStageConfig
    TurnStageConfig --> HookContext
    HookContext --> Hook_t

    TurnStageFactory --> TurnStageConfigParser
    TurnStageRegistrar -.->|registers at static init| GlobalCreators
    TurnStageRegistrar -.->|registers at static init| PerFactionCreators
    TurnStageFactory --> GlobalCreators
    TurnStageFactory --> PerFactionCreators
    GlobalCreators -->|known id| GlobalTurnStage
    PerFactionCreators -->|known id| PerFactionTurnStage
    TurnStageFactory -->|unknown id, repeat=false| CustomGlobalTurnStage
    TurnStageFactory -->|unknown id, repeat=true| CustomPerFactionTurnStage

    TurnStageBase --> GlobalTurnStage
    TurnStageBase --> PerFactionTurnStage
    GlobalTurnStage --> TurnStart
    GlobalTurnStage --> WorldEvents
    GlobalTurnStage --> VictoryConditionChecks
    GlobalTurnStage --> TurnEnd
    GlobalTurnStage --> Save
    GlobalTurnStage --> CustomGlobalTurnStage
    PerFactionTurnStage --> ResourceCollection
    PerFactionTurnStage --> UnitSupport
    PerFactionTurnStage --> BaseProduction
    PerFactionTurnStage --> BaseGrowth
    PerFactionTurnStage --> IncomeCollection
    PerFactionTurnStage --> ResearchAccumulation
    PerFactionTurnStage --> Upkeep
    PerFactionTurnStage --> Population
    PerFactionTurnStage --> PlayerActions
    PerFactionTurnStage --> PostActionsProduction
    PerFactionTurnStage --> Mood
    PerFactionTurnStage --> CustomPerFactionTurnStage

    TurnProcessor --> Advance
    TurnProcessor --> GlobalRegistry
    TurnProcessor --> PerFactionRegistry
    TurnProcessor --> StageOrder
    Advance -->|Yield| Pause[pause until next Advance]
    Advance -->|exception| Abort[OnExit then rethrow; Reset recovers]

    style TurnProcessor fill:#bbf,stroke:#333,stroke-width:4px
    style TurnStageFactory fill:#bbf,stroke:#333,stroke-width:4px
    style TurnStageConfig fill:#ff9,stroke:#333,stroke-width:2px
    style ConfigFile fill:#bfb,stroke:#333,stroke-width:2px
    style GlobalTurnStage fill:#fbf,stroke:#333,stroke-width:2px
    style PerFactionTurnStage fill:#fbf,stroke:#333,stroke-width:2px
```

## Component Overview

### TurnStageBase / GlobalTurnStage / PerFactionTurnStage
(`include/game/TurnStages.h`)

A turn stage never receives a parameter it cannot use: rather than one interface with
nullable `GameState*`/`Faction*` arguments, there are two narrow interfaces:

- **`TurnStageBase`**: shared hook lifecycle (`OnEnter`/`OnExit`). Subclasses may override
  `OnEnterImpl` / `OnExitImpl` for stage-local cleanup (e.g. `PlayerActions` pass state).
  `HasReplaceHooks()` is true only when a replace hook has a **callable** callback —
  unbound replace entries must not suppress `ExecuteImpl`.
- **`GlobalTurnStage`**: `Execute(GameState&)`, once per turn.
- **`PerFactionTurnStage`**: `Execute(GameState&, Faction&)`, once per living faction per turn.

### Yield / resume contract

`StageResult_t::Yield` pauses turn processing. The next `TurnProcessor::Advance` re-enters
the **same** stage (and, for per-faction stages, the **same** faction until that faction
`Continue`s). UI / Engine must not assume turns are atomic — overlays and player input may
sit between `Advance` calls (see UI modal gating; package 2).

Mid-stage player prompts (production abandon, tech notices, …) use the
[player interaction queue](player-interaction-system.md): stages `Enqueue` + `Yield` when
`HasPendingFor` the player; `InteractionPresenter` maps `Front` to Notice / OpenView and
calls `CompleteFront` then `ProcessTurn_` after resolution.
`YieldingPerFactionTurnStage` shares faction-bind and `PlayerHasPending_` for `BaseProduction`
and `PlayerActions` (entity loops stay separate). Enqueuing itself is the free function
`EnqueueForPlayer` next to `PlayerInteractionQueue`, not a stage member: `Population` warns
about a pending riot without being a yielding stage.

`PlayerActions` for a player faction:

1. Queued player interactions `Yield` first, in either phase (`PlayerHasPending_`).
2. First enter of a pass → `Yield` (`AwaitingInteraction`) so the player can issue orders.
3. Resume (End Turn) → resolve pending multi-turn orders; if a unit still needs orders →
   `Yield` again without re-executing units already advanced this pass.
4. When the faction pass `Continue`s (or the stage exits), phase and the advanced-unit set
   reset so a later player still gets the interaction gate.

### TurnProcessor (`TurnProcessor.{h,cpp}`)

- **`Advance(GameState&)`**: runs stages until one yields, wrapping the stage order for the
  next turn cycle. Throws if a full cycle completes with no yielding stage.
- **Per-faction resume**: tracks completed faction ids for the current stage and the
  yielded faction id. Does **not** depend on monotonic id ordering. If the resume faction
  disappears while yielded, remaining incomplete factions are processed.
- **Exceptions**: after `OnEnter`, a throw from `Execute` still runs `OnExit` (post hooks)
  and clears entered/resume state, then rethrows. **`Reset()`** returns the processor to
  stage index 0 for recovery after a poisoned no-yield cycle or aborted stage.
- Erasing a faction mid-stage loop is unsupported until the lifetime protocol defines it —
  see `docs/architecture/high-level.md`, "Object lifetime and ownership transfer", which
  covers unit/base destroy and transfer but explicitly defers faction elimination.

### TurnStageFactory / TurnStageRegistrar

- Built-ins register via `TurnStageRegistrar<T>` into **typed** global or per-faction
  creator maps (`if constexpr` on the base). `CreateStages` does not rediscover kind by RTTI.
- `repeatForEachFaction` in config is cross-checked against the registered kind (mismatch
  throws). For unknown (mod) ids the flag selects `CustomGlobalTurnStage` vs
  `CustomPerFactionTurnStage`.
- Duplicate stage ids throw at parse / create. `scriptPath` hooks throw at parse (no Lua
  loader yet). Custom stages require at least one **callable** callback.

### Built-in stage behaviour (non-exhaustive)

- **`TurnStart`**: increments mission year (`GameState::k_StartingMissionYear` → first
  playable year `k_FirstPlayableMissionYear`), publishes turn-start events, refreshes moves.
- **`WorldEvents`**: forest/kelp spread via `SpreadTerraformImprovements`, using
  `GameState::GetRng()` and `GetYearsSinceFirstPlayableYear()` (session stream — not a
  private year×area seed).
- **`Population`**: composition recalculation, then `ForecastMood` per base — which sets
  *pending* riot / golden-age state and enqueues the player's warning, without applying any
  gameplay effect. Stays after `BaseProduction` because a facility completed this turn can
  change either. A base that starves to nothing is razed by `BaseManager`'s pop-loss handler as
  it happens, not swept for here, so it has already dropped out of `Faction::Bases()` before the
  loop reaches it (see "Object lifetime" in `high-level.md`).
- **`ResourceCollection`**: `ProduceBaseResources(GameState&)` — computes Friendship/Pact
  commerce from the injected `CommerceManager`, then each base's `ProduceResources` adds that commerce to
  raw energy before inefficiency and the econ/labs/psych split.
- **`UnitSupport`**: `ApplyMineralSupport` — home-unit support charged against the mineral
  bank ResourceCollection just filled; surplus units disband.
- **`BaseProduction`**: `ApplyProduction` — allocate leftover minerals to the queued build or
  convert them through a stockpile, then complete funded items. Ordered after `UnitSupport` and
  before `BaseGrowth` so Hab Complex / Dome raise `MaxBaseSize` before growth, and before
  `IncomeCollection` / `ResearchAccumulation` so converted econ and labs are spent this turn.
  Colony-pod abandon uses `WouldGrowThisTurn` and `CommitPendingGrowth` before on-complete pop
  costs. Leftovers after a completion stay on the next item and convert only on a later turn if
  that item is a stockpile. Stockpiles are their own config family (`config/stockpiles.json`,
  `StockpileRegistry`); conversion delegates to `ApplyStockpileConversionAtBase`, which resolves
  the stockpile config's own effects and never touches the base effect pool.
- **`PostActionsProduction`**: after `PlayerActions`, completion-only pass via
  `TryCompleteReadyProduction` (`bNewTurn=false`) — finishes funded queues (e.g. hurried during
  the player window) without banking leftover minerals or clearing same-turn abandon deferrals.
  Shares yield / abandon / pick-next handling with `BaseProduction`. Ordered before `Mood`.
- **`BaseGrowth`**: `ApplyBaseGrowth` — spends the gross nutrient bank `ResourceCollection`
  filled; `PopulationManager` subtracts citizen intake, then grows when tanks were already
  full and net ≥ 0, starves when net exhausts storage, halves tanks when full at the
  population limit, then deposits and caps. Ordered *after* `BaseProduction` so a
  cap-raising facility unlocks growth the same turn.
- **`Upkeep`**: deploy-record pruning and facility energy upkeep (after income). A facility
  completed earlier in this turn is present for upkeep.

  Mineral support and production are separate stages because support must claim the bank first,
  and `turn_stages.json` is where that order is expressed. Exactly one drain of the leftover
  mineral bank happens per turn, inside `BaseProduction`.
- **`PlayerActions`**: interactive yield + idempotent order resolution (above).
- **`Mood`**: `CommitMood` per base — the second half of the split `Population` began.
  Forecast warns *before* `PlayerActions` so the player can still avert a riot by moving
  specialists or psych; commit re-evaluates *after* they had that chance and latches the
  result, ages a probe-forced riot, and advances the consecutive-riot count that selects the
  escalation tier. The active tier's `on_enter_effects` (facility destruction, rebellion)
  fire here. See `docs/architecture/population-system.md` for the mood lifecycle itself.

  The stage keeps pass-local state (`m_committedBaseIds`, cleared in `OnEnterImpl`) because a
  rebelling base changes owner mid-pass: without it the receiving faction's turn through the
  faction loop would commit the same base a second time in one turn.

### Configuration (`config/turn_stages.json`)

Flat JSON array; order is turn order. Each entry: `id`, `name`, `description`,
`repeatForEachFaction`, and `hooks.{pre,post,replace}` (lists of `{modId, scriptPath}`).
Stock config has no unbound Custom/mod sample stage.

### Integration Flow

1. `Engine::InitializeUi_` (the third composition-root phase — see `high-level.md`) loads
   `config/turn_stages.json`, `CreateStages()`, builds `stageOrder` from configs, constructs
   `TurnProcessor`.
2. Each interactive step, `Engine` calls `m_turnProcessor->Advance(*m_gameState)` when the
   UI allows turn advance (modal contract is package 2).
3. For each stage: `OnEnter` → `Execute` (possibly `Yield`) → on `Continue`, `OnExit`.
4. Replace hooks skip `ExecuteImpl` **only** when a replace callback is callable.
