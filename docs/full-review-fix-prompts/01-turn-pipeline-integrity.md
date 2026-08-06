# Package 1 — Turn pipeline integrity: hooks, exceptions, yield/resume

**Date:** 2026-08-06  
**Source:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md) Package 1; findings in [`docs/full-code-review.md`](../full-code-review.md) (Game core — turn pipeline; Turn stages — early / late)  
**Verdict:** **Confirm** the package scope and all listed findings (none fail to reproduce). **Amend** the replace-hook fix (fail loudly + gate on runnable callbacks only; no Lua), the WorldEvents RNG surface (session `GameState` stream + shared year epoch; seed ownership stays with package 4), and Population EOT wiring (call the APIs here; calculator correctness stays with packages 6 / 12). Do **not** split the package.

**Findings that no longer reproduce:** none. Effects packages 1–8 changed `Population` / growth call sites (no more stage-appended world-effect vectors), but every Package 1 claim still holds at the cited loci (line numbers below verified against the current tree).

---

## Verified diagnosis

### 1. Replace hook presence silently deletes `ExecuteImpl` — **confirmed [H]**

```65:72:include/game/TurnStages.h
    StageResult_t Execute(GameState& rGameState)
    {
        if (HasReplaceHooks())
        {
            ExecuteReplaceHooks();
            return StageResult_t::Continue;
        }
        return ExecuteImpl(rGameState);
    }
```

Same shape for `PerFactionTurnStage` at `TurnStages.h:85-92`. `HasReplaceHooks()` is `!m_replaceHooks.empty()` (`HookContext.cpp:62-65`). `ExecuteReplaceHooks` only invokes `hook.callback` when set (`HookContext.cpp:50-59`). Nothing in the repo assigns `Hook_t::callback` (grep: no `callback =`). `TurnStageConfigParser::ParseHooks` records `mod_id` / `script_path` only (`TurnStageConfigParser.cpp:55-61`).

Shipped config already exercises the bug: `CustomModStage` has a replace entry pointing at a Lua path (`config/turn_stages.json:135-148`). That stage constructs, skips empty `ExecuteImpl`, prints a cout line per turn, and returns `Continue`. Adding the same replace list to `Upkeep` / `BaseProduction` would silently skip real work. `ExecuteReplaceHooks()` returns `void` (`HookContext.h:29`), so even a future callback cannot express `Yield` or failure.

### 2. Stage exceptions wedge `TurnProcessor` and skip post hooks — **confirmed [H]**

```31:37:src/game/TurnProcessor.cpp
void TurnProcessor::EnsureEntered_(TurnStageBase& rStage)
{
    if (!m_bStageEntered)
    {
        rStage.OnEnter();
        m_bStageEntered = true;
    }
}
```

`CompleteStage_` (`TurnProcessor.cpp:23-29`) is the only path that calls `OnExit()` / clears `m_bStageEntered`. There is no try/scope guard around `Execute`. A throw leaves pre hooks run, post hooks unrun, and the next `Advance` re-enters the same stage without `OnEnter`. The no-yield throw at `TurnProcessor.cpp:108-109` leaves `m_stageIndex == m_stageOrder.size()` with `m_bStageEntered == false`, so every later `Advance` re-throws — no `Reset` / abort API exists (`TurnProcessor.h`).

### 3. Mid-pass `Yield` re-executes multi-turn unit orders — **confirmed [H]**

```43:71:src/game/stages/PlayerActions.cpp
    for (Unit& rUnit : rUnitManager.Units())
    {
        // ... skip empty / spent move orders ...
        const OrderProgress_t progress = rExecutor.Execute(rUnit);
        // ...
        if (bPlayer && DoesUnitRequireOrders_(rUnit))
        {
            return StageResult_t::Yield;
        }
    }
```

No resume cursor. `TurnProcessor` re-enters the same stage/faction (`TurnStages.h:15-16`, `TurnProcessor.cpp:67-70`). `HoldForTurnsOrder_t` / `TerraformOrder_t` decrement `turnsRemaining` on every `Execute_` (`UnitOrderExecutor.cpp:470-501`). One mid-pass yield after an earlier terraformer/hold unit has already ticked double-counts that order in a single turn.

### 4. WorldEvents private RNG from year × map area — **confirmed [H]**

```23:30:src/game/stages/WorldEvents.cpp
    const int turnIndex = std::max(0, rGameState.GetMissionYear() - 2100);
    const WorldMap& rMap = rGameState.GetWorldMap();
    std::mt19937 rng(static_cast<std::mt19937::result_type>(
        static_cast<unsigned>(turnIndex) * 0x9E3779B9u
        ^ static_cast<unsigned>(rMap.GetWidth() * rMap.GetHeight())));
    SpreadTerraformImprovements(rGameState.GetWorldMap(), rGameState.GetTileEffects(),
                                turnIndex, rng);
```

`GameState` already owns `m_rng` for combat/probes (`GameState.h:192-194`, seeded in `GameState.cpp:48`) with no public accessor. Same map size + same year ⇒ identical forest/kelp sample sequence across saves and independent of the session seed. Signed `GetWidth() * GetHeight()` before the unsigned cast is also a latent UB risk on large maps (hygiene).

### 5. `repeat_for_each_faction` inert for built-ins — **confirmed [M]**

`CreateStageInstance` consults the flag only on the `Custom*` fallback (`TurnStageFactory.cpp:81-92`). Every built-in entry in `config/turn_stages.json` still carries the flag; flipping it on `ResourceCollection` changes nothing. Doc acknowledges the design (`turn-system.md:143`) but nothing rejects a mismatch.

### 6. `CreateStages` RTTI bucketing + silent duplicate ids — **confirmed [M]**

`CreateStages` (`TurnStageFactory.cpp:62-74`) `dynamic_cast`s after the registrar already knew `T` (`TurnStageRegistrar.h:19-22`). Duplicate `config.id` overwrites the map entry while `Engine` still walks the full config list as stage order (`Engine.cpp:397-402`), so one instance can run twice. No duplicate-id rejection at parse or factory.

### 7. Per-faction resume by monotonic id — **confirmed [M]**

```59:70:src/game/TurnProcessor.cpp
    for (Faction& rFaction : rGameState.Factions())
    {
        if (m_resumeFactionId.has_value() && rFaction.GetFactionId() < *m_resumeFactionId)
        {
            continue;
        }
        // ...
        m_resumeFactionId = rFaction.GetFactionId();
```

Justified in `TurnProcessor.h:40-44` by an unenforced `GameState` / `IdAllocator` invariant. Failure mode if order ever diverges: silent faction skip. Range-`for` also invalidates if a faction is erased mid-loop (latent; nothing erases today).

### 8. Shared `m_phase` survives eliminated resume faction — **confirmed [M]**

Player path sets `EndingInteraction` before yield and only resets to `AwaitingInteraction` on that faction's `Continue` (`PlayerActions.cpp:31-35,74-77`). `CompleteStage_` does not touch stage-local state. After a yielded player faction disappears, remaining factions `Continue` and a later player skipsthe interaction yield.

### 9. Population stage skips riot / golden-age EOT — **confirmed [M]**

```19:33:src/game/stages/Population.cpp
    rFaction.ApplyBaseGrowth();
    for (BaseManager& rBase : rFaction.Bases())
    {
        // ...
        rBase.GetPopulation().RecalculateComposition();
    }
    return StageResult_t::Continue;
```

`CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` exist (`PopulationManager.cpp:231-248`) with **no callers** anywhere. Players never enter/leave riot or golden age through the turn pipeline (`ForceRiot` is the only live riot setter today).

**Amendment:** wiring these calls makes package 6's wrong `GetWorkerCount()` golden-age input and package 12's non-sticky `ForceRiot` observable. Still wire the stage here; do not "fix" those calculator bugs inside this package.

### 10. No `PlayerActions` yield/resume tests — **confirmed [M]**

`tests/game/TurnProcessorTests.cpp` covers processor yield/resume with stub stages only. No test under `tests/` exercises `PlayerActions` interaction yield, mid-pass yield, or terraform/hold double-tick.

### 11. Custom stage "at least one hook" guard is hollow — **confirmed [M]**

`RequireAtLeastOneHook` (`CustomTurnStage.cpp:10-15`) checks list non-emptiness only — same incomplete edge as finding 1. Shipped `CustomModStage` passes construction and remains a cout no-op.

### 12. First-playable-year epoch duplicated — **confirmed [M]**

WorldEvents subtracts literal `2100` (`WorldEvents.cpp:23-24`). `GameState.cpp:30-32` owns `k_StartingMissionYear = 2099` so the first `TurnStart` increment lands on 2100 (`TurnStart.cpp:24`). The two are unlinked.

### 13. Turn-system architecture doc predates yield/resume — **confirmed [M]**

`docs/architecture/turn-system.md:97-103,165-176,210-220` still document `ProcessTurn`, a straight walk, placeholder `PlayerActions` / `WorldEvents`, `NewYearBegins`, and Engine-owned year increment. Live API is `Advance` + `StageResult_t::Yield`; `TurnStart` owns the year; `WorldEvents` mutates the map; `PlayerActions` is interactive.

### 14. Convention / hygiene (turn-pipeline + both stage sections) — **confirmed [L]**

Still present: snake_case / uninitialized `repeat_for_each_faction` (`TurnStageConfigParser.h:18`); triplicated hook parse loops and empty hook acceptance (`TurnStageConfigParser.cpp:41-61`); unconditional hook/`stage` `std::cout`; empty user-declared ctors; `protected HookContext m_hookContext` (`TurnStages.h:56`); unused includes; stage `~T() = default`; WorldEvents double `GetWorldMap` / signed area multiply; stub stage duplication. Include in this package's sweep where the file is already touched; do not expand into unrelated Engine/UI hygiene.

**Out of package (same review section, not in Package 1 table):** `EventBridge::WireBase` opt-in wiring — leave for composition / event packages (4 / 13 / 16). Do not pull it into this remediation.

---

## Design decision

### Chosen

**A. Honest hooks without Lua (confirm review; reject "implement the loader")**

1. **Parse-time:** any hook entry with a non-empty `script_path` (or with neither `mod_id` nor a bindable callback) **throws** at config load. Scripts cannot run yet; naming one must not load.
2. **Runtime replace gate:** skip `ExecuteImpl` only when at least one replace hook has a **callable** `callback`. Presence of an empty `Hook_t` must not delete built-in behaviour.
3. **Shipped config:** remove `CustomModStage` (or its replace entry) from the default `config/turn_stages.json` so the stock game loads. Sample mod stages belong with package 16's mod-seam work.
4. **Callback result type:** leave `std::function<void()>` for this package. Document that replace hooks cannot `Yield` / fail until package 16 widens the hook contract. Do not pretend `void` can substitute for `StageResult_t`.

**B. Exception story: unwind runs `OnExit`; processor can return to a known state**

- Wrap stage `Execute` in `ExecuteGlobalStage_` / `ExecutePerFactionStage_` so `OnExit` runs on both success-`Continue` and unwind (scope guard / try/catch that rethrows).
- On exception after enter: clear `m_bStageEntered`, clear `m_resumeFactionId`, and expose `Reset()` (or equivalent) that returns the processor to stage index 0 / not-entered so a poisoned no-yield throw is recoverable.
- Do **not** swallow gameplay exceptions; cleanup then rethrow (or abort the turn with a single documented error type — prefer rethrow after lifecycle restore).

**C. Idempotent `PlayerActions` resume**

- Track which units already advanced this faction pass (unit id set, or resume-after cursor into `Units()`), and only `Execute` the remainder after a mid-pass `Yield`.
- Reset pass-local resume state and `m_phase` whenever the stage completes (`Continue` → processor `CompleteStage_`) **and** on `OnExit` / exception cleanup so an eliminated resume faction cannot leave `EndingInteraction` stuck.
- Keep the existing two-phase player contract (first call yields for interaction; End Turn resumes order resolution) — package 2 consumes that contract, does not redefine it.

**D. Per-faction processor resume without id ordering**

- Resume via a set of already-completed faction ids for the current stage (or a stable index into the current `Factions()` snapshot), not `id < resumeId`.
- Document that erasing a faction mid-stage loop is unsupported until a lifetime protocol (package 3) defines it; do not invent faction deletion here.

**E. Config truth: verify `repeat_for_each_faction`; reject duplicates; bucket at compile time**

- After create+bucket, throw if the flag disagrees with global vs per-faction membership.
- Reject duplicate stage ids at load/`CreateStages` (first failure wins; no silent overwrite).
- Registrar uses `if constexpr (std::is_base_of_v<...>)` (or a typed creator) so `CreateStages` does not rediscover kind by RTTI. Rename config field to project style (`bRepeatForEachFaction` / camelCase JSON key per guidelines — update JSON + parser together; no back-compat alias).

**F. WorldEvents uses session RNG + one year epoch**

- Expose a narrow `GameState` RNG accessor (reference to `m_rng`) and pass that into `SpreadTerraformImprovements`.
- Share one named epoch / helper with `k_StartingMissionYear` (e.g. years since first playable year) so WorldEvents does not hardcode `2100`.
- **Do not** invent a second seed policy or re-seed from year×area. Broader "one seed handed down at composition" (including `FactionFlavor`) stays package 4; this package only stops forking a private stream.

**G. Population stage invokes EOT riot / golden-age checks**

- After composition, call `CheckRiotEndOfTurn` and `CheckGoldenAgeEndOfTurn` per base.
- Do **not** change `RiotCalculator` / `GoldenAgeCalculator` / `GetWorkerCount` semantics here — packages 6 and 12 own those. Tests here assert the stage **calls** the hooks (or observable signal/flag transitions under fixtures that force known calculator inputs).

**H. Architecture doc rewrite for `Advance` / Yield**

- Update `docs/architecture/turn-system.md` to match `Advance`, yield/resume, per-faction resume, exception/`OnExit` invariant, real `PlayerActions` / `WorldEvents` behaviour, and `TurnStart` year ownership. Remove `ProcessTurn` / `NewYearBegins` / placeholder claims that contradict code.

### Rejected

| Alternative | Why not |
|-------------|---------|
| Implement Lua / script loading in this package | Package 16 mod-seam work; package brief forbids unless smallest honest fix — failing closed is smaller and honest. |
| Keep gating on hook *presence* but "document" that replace means delete | Silent gameplay deletion already ships; guidelines require throw over silent wrong behaviour. |
| Leave `CustomModStage` in default config with empty replace removed but stage kept | Custom stage still requires a hook; empty stage fails construction — remove the sample from default order instead. |
| Swallow stage exceptions and `Continue` | Hides half-applied turn state; worse than wedging. |
| Restart the whole turn on any throw | Too coarse; post-hook / enter-exit invariant is the actual bug. Prefer lifecycle restore + rethrow + `Reset`. |
| Re-seed WorldEvents from `GameState` seed each year | Still a private stream forked from year; requirement is one session stream policy. |
| Skip wiring Population EOT until packages 6/12 land | Leaves the turn pipeline incomplete; calculator bugs stay their packages' jobs. |
| Split yield/`PlayerActions` into a separate package | Same state machine as processor exceptions and resume; splitting would re-negotiate the yield contract with package 2 twice. |

### Interactions with other packages

| Package | Interaction |
|---------|-------------|
| **2** Modal / turn gating | This package **defines** yield/resume semantics (`Advance` until `Yield`; `PlayerActions` interaction then order pass). Package 2 must query "may advance" instead of asserting atomic turns (`Engine::ProcessTurn_` overlay throw). Do not change UIManager / WorldView here beyond what is required if a test harness calls `Advance`. |
| **3** Lifetime | Faction erasure during a per-faction loop is latent; document unsupported. Do not implement elimination mid-stage. |
| **4** Composition / session seed | WorldEvents consumes `GameState` RNG; package 4 owns seeding `FactionFlavor` and a single seed source at composition. Coordinate accessor naming if both land close together. |
| **6** Base economy / composition | Golden-age inputs via `GetWorkerCount()` remain wrong until package 6; wiring EOT makes that bug live — do not "fix" counts in the stage. |
| **12** Population calculators | `ForceRiot` stickiness; wiring `CheckRiotEndOfTurn` will clear unsustained forced riots until package 12 fixes the calculator. |
| **16** Mod seams / docs hygiene | Lua binding, replace hooks that return `StageResult_t`, sample mod stage, leftover architecture doc sweep. This package only fails closed and rewrites `turn-system.md`. |

---

## Implementation plan

1. **Hook honesty**
   - Parser: reject empty hook objects and any `script_path` that cannot be bound.
   - `HasReplaceHooks` / NVI gate: runnable callbacks only.
   - Remove `CustomModStage` from default `turn_stages.json`.
   - Tighten `RequireAtLeastOneHook` to require a callable callback (or rely on parse rejection so Custom* cannot be built without one).
2. **`TurnProcessor` lifecycle**
   - Scope-guard `OnExit` on unwind; clear entered/resume state; add `Reset()`.
   - Replace id-ordered per-faction resume with completed-id set (or index).
   - Tests for throw-during-stage → post hooks ran / processor not wedged; `Reset` after no-yield poison.
3. **Factory / config**
   - Compile-time bucket in registrar; delete RTTI cast chain (or reduce to assert).
   - Duplicate id rejection; `repeat_for_each_faction` cross-check; rename field to guidelines.
4. **`PlayerActions`**
   - Per-pass advanced-unit tracking; reset phase + cursor on stage exit / complete.
   - Dedicated yield/resume tests (terraform/hold once across mid-pass yield; phase reset if resume faction gone — simulate by completing stage without player `Continue` path if needed).
5. **`WorldEvents` + year epoch**
   - `GameState` RNG accessor; use it in spread; shared epoch helper with `k_StartingMissionYear`.
   - Stage-level test: two GameStates with different RNG seeds diverge; same seed + same map ops agree (requirement: session stream, not year×area).
6. **`Population`**
   - Call both EOT checks after composition; test that riot/golden-age update runs (signal or flag) when calculator inputs warrant it under controlled fixtures.
7. **Docs + hygiene in touched files**
   - Rewrite `turn-system.md` for `Advance` / Yield / real stage behaviour.
   - Apply listed [L] cleanups in files this package edits (no drive-by across unrelated stages beyond cout/dtor noise if already open).

---

## Test plan

Requirement-based (assert intended rules; do not lock today's silent no-ops):

1. **Replace without callback does not skip built-in**  
   Built-in stage with a replace `Hook_t` that has empty callback still runs `ExecuteImpl`. (If parser rejects such hooks first, assert parse/factory throw instead — either way, silent deletion is impossible.)

2. **`script_path` fails config load**  
   A stage JSON naming `script_path` throws at parse/load. Default `turn_stages.json` loads successfully (no `CustomModStage` replace sample).

3. **Custom stage without callable hook fails at construction/load**  
   Empty or cout-only Custom* cannot sit in the pipeline.

4. **Exception runs post hooks / restores processor**  
   Stub stage: pre hook flag, throw from `ExecuteImpl`, assert post hook ran (or `OnExit` observed) and `m_bStageEntered` is false / `Reset` allows a later `Advance`. Prefer observing via test doubles rather than reading private fields if possible (friend/test hook or public `Reset` + successful subsequent Advance).

5. **No-yield cycle is recoverable**  
   After the logic_error for a non-yielding order, `Reset` + a yielding order can `Advance` again.

6. **Per-faction resume ignores id gaps**  
   Two factions; yield on first; resume continues first then second even if ids are not a dense prefix (construct ids that would break `<` skipping if order differed — or use a completed-set assertion). Requirement: every living faction is processed once per stage.

7. **`PlayerActions` mid-pass yield does not double-tick**  
   Unit A terraform/hold with `turnsRemaining == N`; unit B needs orders → Yield; resume → A's `turnsRemaining == N-1` (not `N-2`).

8. **`PlayerActions` interaction phase**  
   First player enter yields without resolving orders; after End-Turn resume, orders resolve; after `Continue`, next cycle yields for interaction again. If resume faction is removed while yielded, next player still gets `AwaitingInteraction` yield.

9. **`repeat_for_each_faction` mismatch throws**  
   Built-in per-faction id with flag false (or the reverse) fails factory.

10. **Duplicate stage id throws**  
    Two config entries with the same `id` fail load/`CreateStages`.

11. **WorldEvents RNG**  
    Spread draws from `GameState`'s stream; year×area alone does not determine the sequence. Epoch helper matches `k_StartingMissionYear` (first playable year index 0 after first `TurnStart`).

12. **Population EOT**  
    After the Population stage, riot/golden-age end-of-turn update has run for each base (signal fire or state transition under fixture-controlled inputs).

**Existing tests that pinned bugs / outdated contracts and must change:**

| Test / doc assertion | Why it must change |
|----------------------|--------------------|
| None identified that assert "replace presence skips ExecuteImpl" or year×area WorldEvents seeding | N/A — those behaviours were untested. |
| `TurnProcessorTests` "throws if no yielding stage" | Keep the throw requirement; **add** recovery via `Reset` (extend, do not delete the throw assertion). |
| Architecture claims in `turn-system.md` (not a test, but pinned wrong API) | Rewrite to `Advance` / Yield; any future doc-driven work must not reintroduce `ProcessTurn`. |
| If a test later asserts CustomModStage loads from default JSON | Update to expect absence or load failure — requirement is stock config has no unbound replace. |

Do **not** weaken `TurnProcessor` yield/resume tests that already encode the correct Advance contract.

---

## AI implementation prompt

```markdown
# Implement Package 1 — Turn pipeline integrity: hooks, exceptions, yield/resume

You are working in the Alpha Centauri C++ rebuild at `/home/martok/alpha-centauri`.

## Goals

1. **Hooks must not silently delete stage behaviour.** Skip `ExecuteImpl` only when a replace hook has a callable callback. Reject at config load any hook that names a `script_path` (or is otherwise unbound). Remove the shipped `CustomModStage` replace sample from default `config/turn_stages.json`. Tighten Custom* "at least one hook" so an empty/unbound mod stage cannot construct.

2. **`TurnProcessor` exception and resume integrity.** If a stage throws after `OnEnter`, `OnExit` still runs and the processor is not left wedged mid-stage. Provide `Reset()` (or equivalent) so a poisoned processor can return to a known state. Per-faction resume must not depend on monotonic faction-id ordering.

3. **`PlayerActions` yield/resume is idempotent.** Mid-pass `Yield` must not re-`Execute` units that already advanced this pass (terraform / hold `turnsRemaining` ticks once). Reset `m_phase` and pass-local resume state when the stage exits/completes so a disappeared resume faction cannot skip the next interaction gate.

4. **WorldEvents uses the session RNG and one year epoch.** Drive `SpreadTerraformImprovements` from `GameState`'s `m_rng` (expose a narrow accessor). Share the first-playable-year epoch with `k_StartingMissionYear` — no literal `2100` fork.

5. **Config matches reality.** Cross-check `repeat_for_each_faction` against the bucketed stage kind (throw on mismatch). Reject duplicate stage ids. Bucket global vs per-faction at compile time in the registrar (no RTTI rediscovery in `CreateStages`).

6. **Population end-of-turn riot / golden age.** After composition, call `CheckRiotEndOfTurn` and `CheckGoldenAgeEndOfTurn` per base.

7. **Docs.** Rewrite `docs/architecture/turn-system.md` for `Advance`, `StageResult_t::Yield`, per-faction resume, real `PlayerActions` / `WorldEvents` / `TurnStart` behaviour. Apply hygiene cleanups in files you already touch.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md`: SOLID, references over pointers, throw over silent defaults, no legacy/back-compat shims — update JSON keys/call sites/tests together.
- Build and test **only** via `./bd` (never raw cmake/make/ctest).
- Read and follow: `docs/full-review-fix-prompts/01-turn-pipeline-integrity.md` (verified diagnosis + design). Findings origin: `docs/full-code-review.md`.
- **Do not** implement Lua / script loading or widen `Hook_t::callback` to return `StageResult_t` — that is package 16.
- **Do not** redefine the UI modal / overlay turn-gate (package 2). Keep the yield contract: `Advance` runs until a stage yields; `PlayerActions` yields for player interaction then again when a unit needs orders.
- **Do not** fix `GetWorkerCount` / golden-age formulas (package 6) or `ForceRiot` stickiness (package 12). Only wire the Population stage calls.
- **Do not** build a second RNG seeding policy or re-seed WorldEvents from year×area. Package 4 owns broader session-seed composition (`FactionFlavor`, etc.).
- **Do not** pull in `EventBridge::WireBase` work.
- Prefer not to expand Engine/UI beyond what tests require; yield semantics are agreed here for package 2 to consume.

## Analysis reference

`docs/full-review-fix-prompts/01-turn-pipeline-integrity.md`

## Primary files

- `include/game/TurnStages.h`
- `include/game/TurnProcessor.h`, `src/game/TurnProcessor.cpp`
- `include/game/TurnStageFactory.h`, `src/game/TurnStageFactory.cpp`
- `include/game/TurnStageRegistrar.h`
- `include/game/TurnStageConfigParser.h`, `src/game/TurnStageConfigParser.cpp`
- `include/game/HookContext.h`, `src/game/HookContext.cpp`
- `include/game/stages/PlayerActions.h`, `src/game/stages/PlayerActions.cpp`
- `src/game/stages/Population.cpp`
- `src/game/stages/WorldEvents.cpp`, `include/game/stages/WorldEvents.h`
- `include/game/stages/CustomTurnStage.h`, `src/game/stages/CustomTurnStage.cpp`
- `include/game/GameState.h`, `src/game/GameState.cpp` (RNG accessor + shared year epoch)
- `config/turn_stages.json`
- `docs/architecture/turn-system.md`
- Tests: `tests/game/TurnProcessorTests.cpp`; add `PlayerActions` / WorldEvents / Population stage coverage as needed

## Acceptance criteria

- [ ] Unbound / `script_path` hooks fail at config load; default `turn_stages.json` loads; no silent replace no-op for built-ins.
- [ ] Replace gate requires a callable callback; Custom* without a callable hook cannot enter the pipeline.
- [ ] Stage throw: `OnExit`/post hooks run; processor not wedged; `Reset` restores a usable state after the no-yield poison path.
- [ ] Per-faction resume does not use `factionId < resumeId` ordering.
- [ ] `PlayerActions`: terraform/hold ticks once across mid-pass yield; `m_phase` resets on stage complete/exit.
- [ ] WorldEvents draws from `GameState` RNG; year epoch shared with starting-year constant; no private year×area mt19937.
- [ ] Duplicate stage ids and `repeat_for_each_faction` mismatches throw.
- [ ] Registrar buckets by type at compile time; `CreateStages` does not rely on RTTI to choose the registry.
- [ ] Population stage invokes riot and golden-age EOT checks after composition.
- [ ] Requirement-based tests added/updated for the above; `./bd test` passes for affected suites.
- [ ] `docs/architecture/turn-system.md` documents `Advance` / Yield / resume (no `ProcessTurn` as live API).

## Out of scope

- Lua hook runtime, mod script hosting, replace hooks returning `Yield` (package 16).
- UIManager / WorldView modal gating and removing `Engine::ProcessTurn_` overlay assertion (package 2) — except documenting the yield contract they must honour.
- Session-wide seed plumbing for `FactionFlavor` / world gen (package 4), beyond exposing/using `GameState` RNG for WorldEvents.
- Golden-age worker counting, riot calculator stickiness, probe ForceRiot duration (packages 6 / 12).
- EventBridge auto-wiring, faction elimination mid-turn, save/load of TurnProcessor cursors.

## What NOT to do

- Do not implement a Lua loader "so replace hooks work."
- Do not keep gating `ExecuteImpl` on replace-list non-emptiness.
- Do not swallow stage exceptions and continue the turn.
- Do not leave `CustomModStage` with an unbound replace entry in the default turn order.
- Do not re-seed a private `std::mt19937` from mission year and map area.
- Do not "fix" Population EOT by changing calculator formulas in this package.
- Do not weaken existing TurnProcessor yield tests that already assert correct Advance behaviour.
- Do not edit unrelated packages' UI/council/unit systems.
```
