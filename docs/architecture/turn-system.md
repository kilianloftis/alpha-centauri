# Turn System Architecture

```mermaid
graph TB
    subgraph "Composition Root"
        Engine[Engine::Initialize_]
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
        IncomeCollection[IncomeCollection]
        ResearchAccumulation[ResearchAccumulation]
        BaseProduction[BaseProduction]
        Population[Population]
        Upkeep[Upkeep]
        PlayerActions[PlayerActions]
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
    PerFactionTurnStage --> IncomeCollection
    PerFactionTurnStage --> ResearchAccumulation
    PerFactionTurnStage --> BaseProduction
    PerFactionTurnStage --> Population
    PerFactionTurnStage --> Upkeep
    PerFactionTurnStage --> PlayerActions
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

`PlayerActions` for a player faction:

1. First enter of a pass → `Yield` (`AwaitingInteraction`) so the player can issue orders.
2. Resume (End Turn) → resolve pending multi-turn orders; if a unit still needs orders →
   `Yield` again without re-executing units already advanced this pass.
3. When the faction pass `Continue`s (or the stage exits), phase and the advanced-unit set
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
- **`Population`**: growth, composition recalculation, then
  `CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` per base.
- **`PlayerActions`**: interactive yield + idempotent order resolution (above).

### Configuration (`config/turn_stages.json`)

Flat JSON array; order is turn order. Each entry: `id`, `name`, `description`,
`repeatForEachFaction`, and `hooks.{pre,post,replace}` (lists of `{modId, scriptPath}`).
Stock config has no unbound Custom/mod sample stage.

### Integration Flow

1. `Engine::Initialize_` loads `config/turn_stages.json`, `CreateStages()`, builds
   `stageOrder` from configs, constructs `TurnProcessor`.
2. Each interactive step, `Engine` calls `m_turnProcessor->Advance(*m_gameState)` when the
   UI allows turn advance (modal contract is package 2).
3. For each stage: `OnEnter` → `Execute` (possibly `Yield`) → on `Continue`, `OnExit`.
4. Replace hooks skip `ExecuteImpl` **only** when a replace callback is callable.
