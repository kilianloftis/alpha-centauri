## Turn stages — early pipeline

**Files:** `src/game/stages/BaseProduction.cpp`, `include/game/stages/BaseProduction.h`,
`src/game/stages/IncomeCollection.cpp`, `include/game/stages/IncomeCollection.h`,
`src/game/stages/PlayerActions.cpp`, `include/game/stages/PlayerActions.h`,
`src/game/stages/Population.cpp`, `include/game/stages/Population.h`,
`src/game/stages/ResearchAccumulation.cpp`, `include/game/stages/ResearchAccumulation.h`,
`src/game/stages/ResourceCollection.cpp`, `include/game/stages/ResourceCollection.h`,
`src/game/stages/TurnStart.cpp`, `include/game/stages/TurnStart.h`

**Assessment:** The thin orchestration stages (`ResourceCollection`, `IncomeCollection`,
`ResearchAccumulation`, and most of `Population` / `BaseProduction` / `TurnStart`) match the
post-1.9 design: self-registering, NVI `ExecuteImpl`, faction work behind `Faction` /
`BaseManager`. `ResearchAccumulation`'s discover loop closes the old "points forever" gap.
The dominant weakness is `PlayerActions`: it is the only stage with real control-flow state,
and mid-pass `Yield` restarts the unit loop without remembering who already advanced.

### [H] Mid-pass `Yield` re-executes earlier units' multi-turn orders
`src/game/stages/PlayerActions.cpp:43-71` — After `Execute` on a unit, if the player unit
still needs orders the stage returns `Yield` with no resume cursor. `TurnProcessor` re-enters
the same stage/faction (`include/game/TurnStages.h:15-16`, `TurnProcessor.cpp:67-70`) and
`ExecuteImpl` walks `Units()` from the start again. Move orders with no fragments left are
skipped (`PlayerActions.cpp:50-54`), but `HoldForTurnsOrder_t` / `TerraformOrder_t` always
call `UnitOrderExecutor::Execute`, which decrements `turnsRemaining` each call
(`UnitOrderExecutor.cpp:470-501`). One player interaction yield after an earlier terraformer
or timed-hold unit has already ticked therefore double-counts that order in a single turn;
repeated "unit needs orders" yields amplify it. Direction: mark units already advanced in
this pass (or resume after the yielding unit) and only `Execute` the remainder.

### [M] `m_phase` can skip the interaction gate after a yielded player is gone
`include/game/stages/PlayerActions.h:24-30`, `src/game/stages/PlayerActions.cpp:31-35,74-77`
— `m_phase` lives on the single shared stage instance. The player path sets
`EndingInteraction` before any `Yield` and only resets to `AwaitingInteraction` when that
faction's pass returns `Continue`. `TurnProcessor` explicitly allows the resume faction to
disappear while yielded (`include/game/TurnProcessor.h:40-43`); remaining factions then
`Continue` and `CompleteStage_` runs without touching `m_phase`. A later player-controlled
faction therefore skips the initial `AwaitingInteraction` yield and runs order resolution
immediately. Direction: reset `m_phase` whenever the stage completes (or key phase by
faction id).

### [M] Population stage never runs riot / golden-age end-of-turn updates
`src/game/stages/Population.cpp:24-33` — The stage applies growth and recalculates
composition, but never calls `PopulationManager::CheckRiotEndOfTurn` /
`CheckGoldenAgeEndOfTurn`. Those APIs and their calculators/signals are implemented
(`PopulationManager.cpp:231+`) yet still have no caller — the same unwired gap noted in
`docs/code-review-findings.md` §5. Players never enter or leave riot/golden age through the
turn pipeline. Direction: invoke both per base here after composition (or fold them into
`Faction::ApplyBaseGrowth` if that becomes the single population tick).

### [M] No tests for `PlayerActions` yield / resume semantics
Implemented behavior (early interaction yield, mid-pass yield when a unit needs orders,
`DeferDestruction` + `Expended`, AI vs player paths) has no dedicated coverage under
`tests/` (only `TurnStart` is exercised from movement tests). The re-entrancy bug above is
exactly the kind of regression a small fixture would catch. Direction: add cases that yield
after unit A terraforms and unit B needs orders, then resume and assert A's
`turnsRemaining` dropped once.

### [L] Convention and hygiene items
- `src/game/stages/IncomeCollection.cpp:22-27`, `ResearchAccumulation.cpp:22-38`,
  `BaseProduction.cpp:23-44`, `Population.cpp:21-31`, `ResourceCollection.cpp:20`,
  `TurnStart.cpp:27` — stages still use `std::cout` for turn tracing; fine for early bring-up,
  but it is unstructured noise once a real log/event path exists.
- `src/game/stages/ResourceCollection.cpp:4` — `#include "game/faction/base/BaseManager.h"`
  is unused (only `Faction` / `GameState` are referenced).
- `include/game/stages/*.h` — every stage declares `~T() = default;` with nothing to destroy;
  omit unless a custom destructor is required.
- `src/game/stages/BaseProduction.cpp:33-36` — completion log prints the constructable id
  (`CompleteProduction` returns id), while the in-progress branch prints `GetName()`;
  inconsistent for readers matching UI names.

**Observed outside slice:**
- `docs/architecture/turn-system.md:198-220` — still describes `TurnStart` / `PlayerActions` as
  placeholders, lists `NewYearBegins`, and says `Engine` increments the mission year after
  `ProcessTurn`; `TurnStart.cpp:24` and `Engine.cpp:112-113` contradict that.
- `docs/code-review-findings.md:735` — "tech discovery never happens" is stale;
  `ResearchAccumulation.cpp:30-38` already runs the discover loop.
