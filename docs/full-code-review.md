# Full project code review

**Date:** 2026-08-03
**Scope:** All of `src/` except `Engine` (ad-hoc testing catchall). Headers under `include/` reviewed with their implementations.
**Lens:** clean code, simplicity, SOLID, clarity, robustness, maintainability.
**Note:** Many systems are intentionally unimplemented; missing features are not treated as defects.
**Effects model:** Findings about the effects subsystem were extracted to [`docs/effects-model-review.md`](effects-model-review.md) and have since been **remediated** by work packages 1–8 (`docs/effects-fix-packages.md`, analyses in [`docs/effects-fix-prompts/`](effects-fix-prompts/)). The per-section "effects-model findings moved" pointers, and the few findings in this document that those packages also fixed, were removed on 2026-08-06.
**Remediation plan for what remains:** [`docs/full-review-fix-packages.md`](full-review-fix-packages.md).

## Summary

| Severity | Count |
|----------|------:|
| High `[H]` | 50 |
| Medium `[M]` | 163 |
| Low `[L]` | 38 |
| **Total** | **251** |

Parts were produced by per-directory review agents (directories with >8 implementation files split across two agents), plus one architecture-only pass. Effects-model findings live in a separate document.

## Contents

- [Architecture](#architecture) — H=4 M=3 L=1
- [Game core — world state and composition root](#game-core-world-state-and-composition-root) — H=4 M=6 L=1
- [Game core — turn pipeline, hooks, and validators](#game-core-turn-pipeline-hooks-and-validators) — H=2 M=5 L=1
- [Buildings and secret projects](#buildings-and-secret-projects) — H=1 M=7 L=1
- [Planetary Council — runtime and outcomes](#planetary-council-runtime-and-outcomes) — H=2 M=8 L=1
- [Planetary Council — configuration and registries](#planetary-council-configuration-and-registries) — H=0 M=5 L=1
- [Faction — economy, research, social engineering, identity](#faction-economy-research-social-engineering-identity) — H=1 M=5 L=1
- [Faction — military, units, diplomacy, visibility](#faction-military-units-diplomacy-visibility) — H=3 M=6 L=1
- [Base management — BaseManager, home-base index, buildings](#base-management-basemanager-home-base-index-buildings) — H=1 M=3 L=1
- [Base management — population and production](#base-management-population-and-production) — H=2 M=9 L=1
- [Base management — resources and worker assignment](#base-management-resources-and-worker-assignment) — H=1 M=4 L=1
- [Map — runtime world model](#map-runtime-world-model) — H=1 M=5 L=1
- [Map — world generation and config](#map-world-generation-and-config) — H=2 M=4 L=1
- [Orbital systems](#orbital-systems) — H=0 M=2 L=1
- [Population — calculators](#population-calculators) — H=1 M=4 L=1
- [Population — pop types and config](#population-pop-types-and-config) — H=0 M=5 L=1
- [Research — tech registry and costs](#research-tech-registry-and-costs) — H=1 M=3 L=1
- [Social engineering — policies and ratings](#social-engineering-policies-and-ratings) — H=0 M=3 L=1
- [Turn stages — early pipeline](#turn-stages-early-pipeline) — H=1 M=3 L=1
- [Turn stages — late pipeline and custom](#turn-stages-late-pipeline-and-custom) — H=1 M=2 L=1
- [Units — model, orders, movement](#units-model-orders-movement) — H=2 M=5 L=1
- [Units — combat, probes, conquest, morale](#units-combat-probes-conquest-morale) — H=2 M=5 L=1
- [Graphics backend](#graphics-backend) — H=1 M=4 L=1
- [Input backend](#input-backend) — H=2 M=4 L=1
- [Shared libraries](#shared-libraries) — H=1 M=4 L=1
- [UI — manager, views, tile rendering](#ui-manager-views-tile-rendering) — H=2 M=5 L=1
- [UI — base view and displays](#ui-base-view-and-displays) — H=0 M=6 L=1
- [UI — base selector popups](#ui-base-selector-popups) — H=1 M=3 L=1
- [UI — commlinks](#ui-commlinks) — H=1 M=3 L=1
- [UI — council vote](#ui-council-vote) — H=1 M=5 L=1
- [UI — research](#ui-research) — H=0 M=2 L=1
- [UI — satellite / orbital](#ui-satellite-orbital) — H=1 M=4 L=1
- [UI — settings](#ui-settings) — H=0 M=4 L=1
- [UI — social engineering](#ui-social-engineering) — H=1 M=3 L=1
- [UI — style](#ui-style) — H=1 M=2 L=1
- [UI — unit designer](#ui-unit-designer) — H=2 M=4 L=1
- [UI — world map and combat presentation](#ui-world-map-and-combat-presentation) — H=2 M=4 L=1
- [UI — world panels and order input](#ui-world-panels-and-order-input) — H=2 M=4 L=1
---

## Architecture

**Scope:** high-level component structure (not a per-file code review)

**Assessment:** The live composition has a clear spine — `GameDataContext` (definitions) outlives `GameState` (session), factions own bases/units, effects are scope-routed with typed pools, and turn stages self-register into a config-ordered pipeline. Recent boundary fixes (lib↔game purity, ID allocators on `GameState`, `IEffectsProvider` memoization, narrow leaf constructors) are real. The dominant structural risk is that **lifetime, mod, and documentation contracts lag the systems that now mutate the world** (base capture/raze/transfer, mid-turn `Yield`): extension seams and diagrams still describe an earlier, quieter object graph.

### [H] Refresh architecture docs to match the live graph
`.cursor/rules/architecting.md` requires diagrams to stay current; several subsystem docs still describe components and ownership that do not exist. `docs/architecture/faction-system.md` centers a `FactionManager`, claims `TurnProcessor` calls `Faction::ProcessTurn()`, and shows `Military` owning bases/`BaseManager`/`UnitFactory` — reality is `GameState::m_factions` (`include/game/GameState.h:192`), stage-driven turns (`TurnProcessor::Advance`), and `Faction` owning `m_bases` while `Military` holds only designs (`include/game/faction/Military.h:17-22`). `docs/architecture/high-level.md` still features `TileBonusRegistry`/`TileMap`/`FactionFactory`/`SFMLUIManager`/`NullUIManager`/`HookSystem`, and `docs/architecture/turn-system.md` documents `ProcessTurn` rather than `Advance` + `StageResult_t::Yield`. Misleading architecture docs will steer new subsystems into the wrong place.

### [H] Define a lifetime protocol now that bases can die or move
Prior review deferred UI `BaseManager&` invalidation because nothing destroyed bases (`docs/code-review-findings.md` §1.8). That assumption is false: `ExtractBase` / `TransferBaseTo` run from conquest, probes, and diplomacy (`src/game/units/BaseConquestEffects.cpp:115,307`, `ProbeActionEffects.cpp:156`, `DiplomaticActionExecutor.cpp:328`). `ViewFactory::CreateBaseView` still takes a live `BaseManager&` (`include/ui/ViewFactory.h:51-56`) with no weak handle or pop-on-destroy path. Separately, `EventBridge::WireBase` is opt-in (`src/game/EventBridge.cpp:15-23`) — founding wires via `onBaseCreated` (`src/game/Engine.cpp:452`), but capture/transfer rebuilds a base without rewiring, so mod-facing pop events silently vanish. Fill-in of combat/diplomacy will keep creating destroy paths; without one invalidation/wiring rule, dangling views and dead event edges become the default.

### [H] Shape mod seams for a real consumer before more systems bypass them
Guideline moddability (config + Lua hooks) and the two-layer event story are only half-built. `Hook_t::callback` is `std::function<void()>` with no `GameState`/`Faction` (`include/game/HookContext.h:10-15`); parsers fill `scriptPath` but never load a script (`src/game/TurnStageConfigParser.cpp:45-59`, `HookContext.cpp:31-57`). `EventBridge` still TODOs faction signals (`src/game/EventBridge.cpp:11-12`) while docs claim they are bridged (`docs/architecture/event-system.md:89-93`). `EventBus::Publish` walks `m_handlers` without a snapshot (`src/lib/EventBus.cpp:23-26`) — unsafe for the stated third-party handler model. Player actions remain direct UI→manager calls. Continuing to grow gameplay without a sample mod (or command/hook context) will lock in interfaces that cannot host one.

### [H] Reconcile turn `Yield` with the UI lifetime contract
`TurnProcessor` is a resumable state machine (`Advance`, per-faction resume — `include/game/TurnProcessor.h:22-26,40-45`; `PlayerActions` yields — `src/game/stages/PlayerActions.cpp:31-34,68-70`), but `Engine::ProcessTurn_` still asserts “no overlay” as if turns were atomic (`src/game/Engine.cpp:98-113`), and `docs/architecture/ui-system.md:217-225` / prior §1.8 status text still describe `ProcessTurn` as unreachable under overlays. Combat already pushes overlays from `WorldView` and skips advance while they are open (`src/ui/world/WorldView.cpp:274-278`). Mid-turn interactive stages and overlay-held live references are the same problem domain; the architecture needs one explicit rule (when yield is allowed, what may be on the stack, what may be destroyed) before more yielding stages land.

### [M] Split composition-root lifecycles (app / new game / load)
`LoadGameData` centralizes registry load+validate (`include/game/GameDataContext.h:85-92`), which is good, but `Engine::Initialize_` still owns world gen, faction seeding, event wiring, turn pipeline, UI shortcuts, and the first `Advance` in one method (`src/game/Engine.cpp:116-472`). Session services also take late `SetGameDataContext` after `GameState` construction (`Engine.cpp:140-141`). As save/load and alternate start modes appear, this single script will fork inconsistently with `tests/GameFixtures.h` (already a divergent composition path per prior review).

### [M] Keep `GameState` a session boundary, not an unbounded service bag
`GameState` correctly owns map-lifetime collaborators (`WorldMap`, `TileEffectsContext`, movement/path stack, `UnitOrderExecutor` — `include/game/GameState.h:182-198`) and world-scoped diplomacy/council. It also owns the mod-facing `EventBus`, orbital helpers, and implements `IUnitOrderWorld`. That is workable as a façade, but every new cross-faction feature currently lands here; without a deliberate “session services vs pure save data” split, serialization and test harnesses will keep growing ad-hoc `Set*` injection points.

### [M] Align faction-subsystem docs with inter-faction ownership
Live diplomacy is world-scoped (`DiplomacyLedger` on `GameState`, `include/game/GameState.h:190`), which matches multi-faction state. Docs still narrate a per-faction `Diplomacy` subsystem (`docs/architecture/faction-system.md:66-71,244-251`) and research as a “global singleton” registry (`docs/architecture/research-system.md:77`) while registries live on `GameDataContext`. The real pattern (faction-local managers + game-state ledgers + context registries) is sound; the documentation pattern is not.

### [L] Architecture hygiene leftovers
- `docs/architecture/high-level.md:247-262` / turn doc still say `ProcessTurn`; code is `Advance` (`include/game/TurnProcessor.h:26`).
- Event ownership: docs say Engine owns `EventBridge` (true — `include/game/Engine.h:47`) but understate that `EventBus` is owned by `GameState` (`include/game/GameState.h:181`).
- UI docs still describe abstract `UIManager` + SFML/Null variants (`docs/architecture/high-level.md:324-336`); `UIManager` is a concrete class (`include/ui/UIManager.h:14-19`).
- `TileBonusRegistry` / `config/tile_bonuses.json` in high-level map section are obsolete; bonuses are improvement placement via world-gen decoration (`TileBonusGeneration`).

---

## Game core — world state and composition root

**Files:** `src/main.cpp`, `src/game/GameState.cpp`, `include/game/GameState.h`,
`src/game/Faction.cpp`, `include/game/Faction.h`, `src/game/GameDataContext.cpp`,
`include/game/GameDataContext.h`, `src/game/GameSettings.cpp`, `include/game/GameSettings.h`,
`src/game/GameCategory.cpp`, `include/game/GameCategory.h`, `include/game/GameDataPaths.h`,
`include/game/GameRulesConfig.h`, `include/game/IConstructable.h`,
`include/game/IEffectsProvider.h`, `include/game/VisibilityConfig.h`

**Assessment:** Lifetime management in this slice is genuinely good: member declaration order
in `GameState`/`Engine` is deliberate and documented, `DerefView` keeps the owning
`unique_ptr`s private, and `IEffectsProvider` / `GameDataPaths` / `VisibilityConfig_t` are
small, honest abstractions. The dominant weakness is that both central classes have grown
into facades that every new subsystem must edit, and that object validity is still
established in stages after construction — `GameDataContext` is default-constructed empty
and filled by a free function, and `Faction` is re-wired in four steps by
`GameState::AddFaction`. The small config/enum files are clean apart from two hand-maintained
duplicates and one back-compat branch the guidelines forbid.

### [H] `GameDataContext` is still a service locator: a default-constructed bag of nullable pointers
`include/game/GameDataContext.h:46` — the struct declares 22 default-null `unique_ptr`
members and a defaulted constructor; the object only becomes usable after the free function
`LoadGameData` (`src/game/GameDataContext.cpp:42`) mutates every field. Nothing in the type
expresses which fields a given consumer requires, so each consumer invents its own answer:
`Faction`'s constructor dereferences one unchecked (`src/game/Faction.cpp:55`,
`*rDataContext.moraleCalculator` — undefined behavior if unset) while passing four others as
possibly-null `.get()` raw pointers (`src/game/Faction.cpp:50-58`), and
`Faction::GetDiscoveredBuildings` throws on a null registry
(`src/game/Faction.cpp:516`). Partially-populated contexts are not hypothetical:
`tests/GameFixtures.h:69` builds one field by field, so the unspecified "half-loaded" state is
routinely exercised and the null-tolerance of every downstream subsystem is load-bearing.
Prior finding 1.3 was closed only for the leaf consumers (`BaseManager` etc.); the root object
was left as-is, so the seam is still a locator rather than an injection point. Direction: make
`LoadGameData` a factory that returns a fully constructed context (or take `GameDataPaths` in
the constructor) and hold the members by value/reference so "null" is unrepresentable.

### [H] `Faction` is only half-constructed until `GameState::AddFaction` finishes wiring it
`src/game/GameState.cpp:145-181` — a `Faction` returned by its own constructor has no world
map, no settings, and no observers; `AddFaction` then applies `SetSettings`, `BindWorldMap`,
`SetOnBaseListChanged`, `SetOnVisibilityRebuilt` and two signal connections. The failure mode
is silent, not loud: `Faction::RebuildVisibility` returns early when `m_pWorldMap` is null
(`src/game/Faction.cpp:596`), so a base founded on an unwired faction produces no visibility,
no territory rebuild and no first-contact check while every getter still returns plausible
values — the opposite of the project's "throw on unexpected null" rule. The wiring order is
also load-bearing and undocumented: `BindWorldMap` (which rebuilds visibility) runs before
`SetOnVisibilityRebuilt` is installed and before the faction is pushed into `m_factions`, so a
faction that arrives already populated (load-game, or any future runtime creation) is never
scanned for contact or territory. This is a fresh instance of prior finding 4.2, which was
recorded as fixed for `Faction`/`GameState`. Direction: take the map, settings and the
`GameState` back-reference in `Faction`'s constructor, and make `GameState` the only thing
that can mint one.

### [H] `GameState` and `Faction` are god-facades that every new subsystem must edit
`include/game/GameState.h:40-207` declares roughly fifty public members spanning mission year,
settings, event bus, faction registry, diplomacy, ID allocation, world map, base lookup,
combat interception, base conquest, effect collection, pathfinding, order execution, probe
actions, first contact, council, territory, secret projects and orbital warfare. Several are
pure forwarders that add only `*this` and `m_rng` to a free function
(`src/game/GameState.cpp:429-452`, `308-325`). `include/game/Faction.h:52-258` has the same
shape at ~60 methods: it owns eight subsystems and *also* keeps its own ad-hoc state and rules
inline — the ASAT/intercept cooldown vector (`include/game/Faction.h:250-258`) and a
faction-wide building inventory (`CountBuildings`, `FindBaseWithBuilding`,
`FindOwnedBuildingConfig`, `CountReadyBuildings`) that belongs in a subsystem next to
`Military`/`BuildingManager`. Every feature added so far has been added by growing these two
headers, which is a direct OCP violation at the busiest merge point in the codebase.

### [H] Ownership transfer is implemented as destroy-then-recreate, inheriting destruction's side effects
`src/game/Faction.cpp:341-397` — `ExtractUnit` routes through `UnitManager::DestroyUnit`
(`src/game/faction/UnitManager.cpp:71`), which applies carrier-loss rules to the unit's cargo
(disembark if the tile is holdable, otherwise destroy) and emits `OnUnitDestroyed`;
`UnitSnapshot_t` (`include/game/units/Unit.h:25-34`) carries no carrier or cargo link. So
subverting a loaded transport (`src/game/units/ProbeActionEffects.cpp:180`) applies
"ship sunk" semantics to a ship that was not sunk, and every observer — including
`GameState`'s revealed-unit cleanup (`src/game/GameState.cpp:167`) and any mod on the event
bridge — sees "destroyed" followed by "created" rather than a change of owner. Both transfer
paths are also destructive on failure: `TransferUnitTo` (`src/game/Faction.cpp:383`) and
`TransferBaseTo` (`src/game/Faction.cpp:322`) have already extracted (and thus destroyed) the
object when `CreateFromSnapshot` throws, with no rollback. Direction: give `UnitManager` a
detach-without-destroy path, or make the snapshot round-trip complete (carrier/cargo) and
non-destructive.

### [M] Building deploy cooldowns are never pruned and leak across base transfer
`src/game/Faction.cpp:192-209` — `DeployBuilding` appends a record that nothing ever removes
on expiry, so `m_buildingDeploys` grows for the whole game and `CountReadyBuildings`
(`src/game/Faction.cpp:178`) rescans it every query. `NotifyBuildingDestroyed`'s comment says
it prefers dropping a "still cooling" record, but its predicate matches on `buildingId` only,
so it usually erases a long-expired record instead — the code does not do what the comment
promises. Worse, `ExtractBase`/`TransferBaseTo` do not drop the records for buildings that
left with the base, so a faction that loses an ODP-bearing base keeps a phantom cooldown that
will suppress a rebuilt copy years later. The root cause is that per-copy cooldown state is
keyed by building id with no per-instance identity and is parked on `Faction`.

### [M] `main.cpp` has no top-level error handling
`src/main.cpp:4-9` — `LoadGameData` and every config parser throw by design (this is the
project's chosen failure mode), and both `Engine`'s constructor and `Run()` propagate. With no
`try`/`catch` in `main`, the single most likely user-visible failure of a moddable,
config-driven game — a typo in a JSON or Lua file — produces `std::terminate` and an abort
with no message. Wrapping `Run()` and reporting `what()` with a non-zero exit code is a
three-line fix in the one place that should own it.

### [M] `GameSettings` loads world-generation values from a user-editable file without validating them
`src/game/GameSettings.cpp:14-30` — `width`, `height`, `oceanCoverage`, `presetId` and `seed`
are taken verbatim from `user_settings.json` with no range or referential check, even though
`MapGenerationConfig_t` documents `oceanCoverage` as `[0,1]`
(`include/game/map/MapGenerationConfig.h:27`). `Engine` feeds these straight into `WorldMap`,
whose constructor does not validate either (`src/game/map/WorldMap.cpp:8`), so a hand-edited
`"width": 0` or `-1` becomes a degenerate or allocation-failing world rather than a clear
error. `Load` is the trust boundary and should throw here.

### [M] Back-compat branch and struct/file layout drift in `GameSettings`
`src/game/GameSettings.cpp:32-57` — `LoadGameRules_` keeps a fallback for "older prefs [that]
stored pause at the top level", and `LoadVisibility_` deliberately reads `remove_shroud` from
`game_rules` and `remove_fog` from `debug_options` "so existing user_settings.json files
continue to load". The guidelines explicitly forbid keeping code for backwards compatibility,
and `Save` (`src/game/GameSettings.cpp:132-142`) has never written the top-level form, so that
branch is reachable only for files no current build produces. The lasting cost is the second
half: the on-disk grouping no longer matches `VisibilityConfig_t`, so the next visibility knob
has a non-obvious home and the Save/Load pair must be edited in lockstep to stay consistent.

### [M] `k_GameCategoryCount` is a hand-maintained duplicate of the enum
`include/game/GameCategory.h:19-26` — the count and the `k_AllGameCategories` array restate
the enumerators by hand while the `.cpp` already uses `magic_enum` for everything else.
`ResearchSelector` sizes `std::array<bool, k_GameCategoryCount>` and indexes it by the cast
enum value (`include/game/faction/ResearchSelector.h:40`,
`src/game/faction/ResearchSelector.cpp:43`), so adding a fifth category compiles cleanly and
writes out of bounds. `magic_enum::enum_count<GameCategory_t>()` removes the second source of
truth; `k_AllGameCategories` has no users at all and should just go.

### [M] The `IUnitOrderWorld` overrides hand `GameState` its own RNG and tile-effects back
`src/game/GameState.cpp:308-325` — `TryInterceptAttack` and the two conquest resolvers receive
`std::mt19937&` and `TileEffectsContext&` parameters and forward them, but `GameState` already
owns both (`m_rng`, `m_pTileEffects`) and its sibling `TryAttackSatellite`
(`src/game/GameState.cpp:445`) reaches for `m_rng` directly. Two routes to the same state mean
a caller that supplies a different generator silently forks the deterministic roll stream
without any compile error. Each override also shares its exact name with the free function it
delegates to, so dropping the `ac::` qualifier turns the call into unbounded recursion.
Removing the parameters requires a matching edit in `include/game/units/IUnitOrderWorld.h`.

### [L] Convention and hygiene items
- `src/game/GameState.cpp:256` — `AllocateBaseId` is defined returning `int` while declared as
  `BaseId_t`; use the alias in both places.
- `include/game/GameState.h:185` — `m_worldMap` is the only owning pointer member without the
  `p` prefix every sibling uses (`m_pTileEffects`, `m_pPathfinder`, …).
- `include/game/GameSettings.h:19` — `kDefaultPath` should be `k_DefaultPath` per the `k_`
  constant convention.
- `src/game/GameState.cpp:311` vs `:442` — `ac::` and `::ac::` qualification used
  interchangeably in the same file.
- `src/game/Faction.cpp:62` — `~Faction() {}` with an empty body where `GameState` uses
  `= default` for the identical out-of-line-destructor purpose.
- `src/game/Faction.cpp:108`, `:216`, `:462`, `:475` — `if (pBase)` guards over a vector whose
  only insertion point (`AddBase`, `src/game/Faction.cpp:236`) already throws on null; the
  neighbouring loops that use `Bases()` have no such guard.
- `src/game/Faction.cpp:455-481` — `ProduceBaseResources` and `ApplyBaseGrowth` are identical
  apart from the per-base call; likewise five near-duplicate "sum over bases" loops split
  between raw `m_bases` and `Bases()`.
- `src/game/GameCategory.cpp:33-45` — `ParseGameCategory` hand-rolls case-insensitive matching
  and lowercases every enumerator name on every iteration;
  `magic_enum::enum_cast<GameCategory_t>(s, magic_enum::case_insensitive)` is the one-liner.
- `src/main.cpp:1` — `<iostream>` is included and unused.
- `src/game/Faction.cpp:1-33` — the include block puts the file's own header second and
  interleaves `<algorithm>` / `<iostream>` among the game headers.
- `src/game/Faction.cpp:447` — `CreateBase` writes to `std::cout` from domain logic; this adds
  to the known logging-seam debt recorded as deferred in the prior review, not a new problem.
- `include/game/IConstructable.h:21` — the method is `GetBaseCost()`, but
  `docs/architecture/high-level.md:269` and `docs/architecture/faction-system.md:259` both
  still document it as `GetMineralCost()`.

**Observed outside slice:**
- `src/game/units/MovementRules.cpp:135` — unit placement legality depends on a mutable
  file-scope global (`s_bSingleUnitPerTile`) rather than config or injected rules.
- `src/game/map/WorldMap.cpp:8` — the constructor accepts any width/height, including zero and
  negative, and `reserve(width * height)` on a negative product is a trap.

---

## Game core — turn pipeline, hooks, and validators

**Files:** `src/game/TurnProcessor.cpp`, `include/game/TurnProcessor.h`,
`src/game/TurnStageFactory.cpp`, `include/game/TurnStageFactory.h`,
`src/game/TurnStageConfigParser.cpp`, `include/game/TurnStageConfigParser.h`,
`include/game/TurnStageRegistrar.h`, `include/game/TurnStages.h`,
`src/game/HookContext.cpp`, `include/game/HookContext.h`,
`src/game/EventBridge.cpp`, `include/game/EventBridge.h`,
`src/game/EffectReferenceValidator.cpp`, `include/game/EffectReferenceValidator.h`,
`src/game/RequiredTechValidator.cpp`, `include/game/RequiredTechValidator.h`

**Assessment:** The stage-type half of the extensibility story is genuinely good: prior finding
1.9's fix landed cleanly, the two narrow `Execute` interfaces mean no stage sees a parameter it
cannot use, `TurnStageRegistrar<T>` really does let a new built-in stage be added without editing
`TurnStageFactory.cpp`, and the `ac-turn-stages` OBJECT library correctly closes the static-init
drop hazard that design would otherwise carry. The dominant weakness is that the *config-driven*
half is unfinished in a way that silently changes behavior rather than failing: a declared
replace hook removes a stage's real work and substitutes nothing. Secondary to that,
`TurnProcessor` gained a yield/resume state machine with no exception story — any throw from a
stage leaves it wedged mid-stage with the hook lifecycle half-run.

### [H] A configured replace hook silently deletes the stage's behavior
`include/game/TurnStages.h:67-71` (and `:87-91`) skip `ExecuteImpl` entirely whenever
`HasReplaceHooks()` is true, and return `Continue`. But `HookContext::ExecuteReplaceHooks`
(`src/game/HookContext.cpp:50-60`) only runs `hook.callback`, and nothing in the repository ever
assigns `Hook_t::callback` — `TurnStageConfigParser::ParseHooks` (`src/game/TurnStageConfigParser.cpp:55-61`)
records `mod_id` and `script_path` and never loads the script. So the presence of a `replace`
entry in `turn_stages.json` is sufficient to turn a built-in stage into a no-op that prints a
line to stdout. `config/turn_stages.json` already ships such an entry (`CustomModStage`), and
adding one to `Upkeep` or `BaseProduction` would silently skip upkeep or production with no
error. This is the actionable core of the still-open prior finding 1.10: the fix is not "write
the Lua loader" but "stop gating `ExecuteImpl` on hook *presence*" — gate on a hook that can
actually run, and throw at config load when a hook names a `script_path` nothing can execute.
Related and worth fixing at the same time: `ExecuteReplaceHooks()` returns `void`
(`include/game/HookContext.h:29`), so a replace hook can never express `Yield` or failure — it
cannot substitute for the interface it replaces even once callbacks exist.

### [H] An exception from a stage wedges TurnProcessor and skips the post hooks
`EnsureEntered_` (`src/game/TurnProcessor.cpp:31-38`) sets `m_bStageEntered = true` before
`Execute` runs, and `CompleteStage_` (`:23-29`) is the only path that calls `OnExit()`. There is
no scope guard and no `try`. If a stage throws mid-turn — and stages reach code that throws
routinely, e.g. `PlayerActions` driving `UnitOrderExecutor` — post hooks never run, and
`m_bStageEntered` stays set, so the next `Advance` re-enters the *same* stage without running its
pre hooks, on top of half-applied turn state. `TurnStageBase`'s documented lifecycle (an `OnEnter`
is followed by an `OnExit`) is therefore only an invariant on the happy path. The same class of
problem applies to the processor's own throw at `:108`: it leaves `m_stageIndex == m_stageOrder.size()`
with `m_bStageEntered` false, so every subsequent `Advance` re-throws immediately without running
anything — the object is permanently poisoned with no `Reset`/abort entry point. Direction: wrap
the stage call so `OnExit` runs on unwind (or explicitly document and enforce "a throwing stage
aborts the turn"), and give the class a way to return to a known state.

### [M] `repeat_for_each_faction` is inert for every built-in stage, with no cross-check
`TurnStageFactory::CreateStageInstance` (`src/game/TurnStageFactory.cpp:81-92`) consults
`config.repeat_for_each_faction` only on the `Custom*` fallback path; for a registered id the
creator's C++ base class decides the shape and the flag is ignored. Every built-in entry in
`config/turn_stages.json` nonetheless carries the flag, so the file states a fact the engine does
not honor: setting `"repeat_for_each_faction": false` on `ResourceCollection` changes nothing.
`docs/architecture/turn-system.md:143` acknowledges the design, but nothing rejects a config that
contradicts the C++ type, which is exactly the desync a modder will hit first. Cheap fix: after
the bucketing in `CreateStages`, compare the resulting registry against `config.repeat_for_each_faction`
and throw on mismatch, making the flag either authoritative or verified.

### [M] `CreateStages` rediscovers the stage kind by RTTI and silently collapses duplicate ids
`src/game/TurnStageFactory.cpp:62-74` runs `DynamicUniquePtrCast<GlobalTurnStage>` then
`<PerFactionTurnStage>` on a `unique_ptr<TurnStageBase>` whose static type the registrar
(`include/game/TurnStageRegistrar.h:19-22`) already knew as `T`. This is part of prior finding
1.9's recorded fix, and it is the piece of that fix that will cost later: a third stage kind
means editing the cast chain, `TurnStageRegistries_t`, and `TurnProcessor::ExecuteCurrentStage_`.
`if constexpr (std::is_base_of_v<GlobalTurnStage, T>)` in the registrar would pick the bucket at
compile time and delete the RTTI step and the "neither" branch. Separately, both registry inserts
use `registries.global[config.id] = ...`, so two config entries sharing an `id` silently overwrite
— the second entry's hooks win, and since `Engine` builds the stage order 1:1 from the config
list, the *same* instance then runs twice per turn. Nothing anywhere rejects duplicate ids
(`JsonConfigLoader::LoadFile` does not check them). This also means a stage cannot legitimately
appear twice in a turn order with different hooks, which is a plausible mod request.

### [M] Per-faction resume relies on an ordering invariant no one enforces
`ExecutePerFactionStage_` (`src/game/TurnProcessor.cpp:59-72`) resumes by skipping every faction
whose id is `< *m_resumeFactionId`. `include/game/TurnProcessor.h:40-44` justifies this by
asserting that ids are monotonically allocated and `Factions()` stays in insertion order — true
today, but it is an invariant owned by `GameState`/`IdAllocator`, unenforced here, and the failure
mode if it ever breaks (turn-order shuffling, load-game reconstruction) is silent: factions are
skipped for a turn rather than anything throwing. Tracking a positional cursor or a set of
already-processed ids for the current stage would make the resume independent of id ordering.
The same comment reasons about the resume faction being "eliminated while yielded"; note that a
stage erasing a faction *during* the loop would invalidate the range-`for` outright, which the
comment does not mention. Nothing erases factions today, so that half is a latent trap, not a
live bug — but the comment currently gives more confidence than the code earns.

### [M] The turn-system architecture doc predates the yield/resume contract
`docs/architecture/turn-system.md:101-103,168` still documents `TurnProcessor::ProcessTurn(GameState&)`
as the entry point and describes it as a straight walk over `m_stageOrder`. That method no longer
exists; the real API is `Advance(GameState&)` with `StageResult_t::Yield`, mid-stage re-entry,
per-faction resume, and a throw when a full cycle produces no yield — none of which appear in the
doc at all. Since `architecting.md` makes keeping these diagrams current mandatory, and since the
yield contract is the single thing a new maintainer most needs before writing a stage, the doc is
now actively misleading. (The fix here lives in the doc rather than the source, but it is the
direct consequence of the change to `TurnProcessor.{h,cpp}`.)

### [M] EventBridge wiring stays opt-in, and now has more call sites to forget
`EventBridge::WireBase` (`src/game/EventBridge.cpp:15-24`) must be invoked by every code path that
creates a base, or that base emits no `EvBaseGainedPop`/`EvBaseLostPop` at all. This is open prior
finding 1.6, and it has gotten slightly worse rather than better: where the prior review recorded
one call site, `Engine` now has two (`Engine.cpp:188` and a `onBaseCreated` callback at `:452`),
so the pattern is spreading instead of being closed. The bridge is the right place to fix it —
subscribing to a faction-level base-created signal from inside `EventBridge` would make wiring a
property of the bridge instead of a step every future caller must remember. The lambdas also
capture `this` with no way to disconnect, which is safe only because `Engine` destroys
`m_eventBridge` after `m_pGameState`; that ordering is not stated anywhere near `WireBase`.

### [L] Convention and hygiene items
- `include/game/TurnStageConfigParser.h:18` — `bool repeat_for_each_faction;` breaks three rules at once: snake_case instead of camelCase, no `b` prefix, and no initializer (indeterminate on a default-constructed `TurnStageConfig_t`).
- `src/game/TurnStageConfigParser.cpp:41-61` — three byte-identical loops differing only in the JSON key and the `Add*Hook` call; one helper taking the key and a member-function pointer removes the triplication. The loop variable is named `hookId` but holds a hook object, not an id.
- `src/game/TurnStageConfigParser.cpp:44-45` — `value("mod_id", "")` / `value("script_path", "")` silently accept a hook entry with neither field, producing a hook that can never do anything; per the guidelines this should throw.
- `src/game/HookContext.cpp:30,42,54` — unconditional `std::cout` per hook, per stage, per turn; there is no logging facility in the project, so this is per-turn stdout spam once any hook is configured.
- `src/game/TurnStageFactory.cpp:60` — "Registered stage" is printed before the bucketing succeeds, so it also prints for a stage that immediately throws at `:72`.
- `src/game/TurnStageFactory.cpp:35-37`, `src/game/HookContext.cpp:7-9`, `src/game/TurnStageConfigParser.cpp:11-13` — empty user-declared constructors, plus matching `~X() = default` declarations (`TurnStageFactory.h:25`, `HookContext.h:21`, `TurnStageConfigParser.h:26`); all four are members added without a requirement.
- `include/game/TurnStages.h:56` — `HookContext m_hookContext` is `protected`, defeating the narrow `HasReplaceHooks`/`ExecuteReplaceHooks` accessors declared two lines above it; `CustomTurnStage.cpp:23` reaches for the member directly.
- `include/game/TurnStageFactory.h:38` — `CreateStageInstance` is a non-static member that touches no member state.
- `include/game/TurnStageConfigParser.h:3,7` — `game/TurnStages.h` and `<memory>` are included but unused by this header.

**Observed outside slice:**
- `docs/architecture/high-level.md:340,226` — refers to a `HookSystem` component that does not exist; `config/turn_stages.json` is loaded by `TurnStageFactory`.
- `docs/code-review-findings.md:178-182` — prior finding 1.8's status note assumes turn processing cannot yet pause mid-turn for player input; `StageResult_t::Yield` now implements exactly that, so the `HasOverlayView()` assertion it describes in `Engine::ProcessTurn_` needs re-examining against the yield path.

---

## Buildings and secret projects

**Files:** `src/game/buildings/BuildingConfigParser.cpp`, `include/game/buildings/BuildingConfigParser.h`, `include/game/buildings/BuildingRegistry.h`, `src/game/buildings/SecretProjectAvailabilityCalculator.cpp`, `include/game/buildings/SecretProjectAvailabilityCalculator.h`

**Assessment:** This is a small, readable slice: the parser is 20 lines of straight-line field
reads on top of the shared `JsonConfigLoader`/`ConfigFields` helpers, and the registry gets
duplicate detection and throwing lookups for free from `Registry<>`. The dominant weakness is
that neither file defends the rules it owns — the parser accepts anything the JSON happens to
contain and defaults the rest, `BuildingRegistry` declines the `Validate_` hook that exists
precisely for entry-level invariants, and the secret-project rule is only ever asked about at
menu-build time, so nothing enforces it at the moment a project is actually completed.

### [H] Secret-project uniqueness is only checked when the build menu is generated
`src/game/buildings/SecretProjectAvailabilityCalculator.cpp:16` — `IsCompleted` has exactly one
production caller: `BuildingManager::GetBuildingsAvailableForConstruction`
(`src/game/faction/base/buildings/BuildingManager.cpp:79-83`), which only filters the list the UI
shows. The path that actually grants a building —
`ProductionManager::CompleteProduction` → `BaseManager`'s `OnProductionCompleted` handler
(`src/game/faction/base/BaseManager.cpp:107-115`) → `BuildingManager::AddBuilding`
(`.../BuildingManager.cpp:24-31`) — performs no check at all. Two bases that both had the
project in their list when they selected it (same faction, or two factions in the same
`BaseProduction` pass) will both complete it, violating the stated rule "only one faction in the
world may own this building" (`config/buildings/README.md:22`). The same hole lets a base finish
a project that was tombstoned by `MarkSecretProjectDestroyed` after it was selected. The fix is
to make this calculator the authority consulted at grant time as well — e.g. give it a throwing
`RequireBuildable(id)` that `AddBuilding` calls — rather than leaving the rule to a UI filter.

### [M] `category` is mandatory in the parser, undocumented, and never read
`src/game/buildings/BuildingConfigParser.cpp:27` — `ParseGameCategoryField` uses `j.at(key)`
(`src/game/GameCategory.cpp:47`), so `category` is a hard requirement, yet the documented schema
does not list it (`config/buildings/README.md:17-24`) and neither documented example includes it.
A modder copying the README example gets a bare
`[json.exception.out_of_range.403] key 'category' not found` at startup. Meanwhile
`BuildingConfig_t::category` is written here and read nowhere in `src/` — only
`TechConfig_t::category` feeds anything (`src/game/faction/ResearchSelector.cpp:121`); the sole
reader of the building field is `tests/game/GameCategoryParserTests.cpp:41`. Either give it a
consumer and document it, or drop the field and stop making every building config carry it.

### [M] Typo'd or wrong-shaped keys are silently defaulted instead of rejected
`src/game/buildings/BuildingConfigParser.cpp:28-31` — every optional field goes through
`json::value(key, default)`, which cannot distinguish "absent" from "misspelled". `"minerals_cost"`
yields `mineralCost == 0` and a building that costs the clamped minimum of 1 mineral
(`ProductionCostCalculator.cpp:18`); `"secretproject"` yields a Secret Project that everyone can
build; `"orbital"` misspelled removes a satellite from the public census. This is the mod-facing
entry point for a whole directory of merged JSON files, and it contradicts the project's
"prefer throwing over returning default values" rule. The parser already demonstrates the right
shape for one key (the `required_techs` rejection at `:32-37`) — generalise it by rejecting any
key not in the known set.

### [M] Parse failures name neither the building nor the file
`src/game/buildings/BuildingConfigParser.cpp:25-39` — `ParseId`, `ParseGameCategoryField`, and the
`value()` type-mismatch path all propagate raw nlohmann exceptions. Since `LoadPath` merges every
`*.json` in `config/buildings/` (`include/lib/config/JsonConfigLoader.h:59-82`), the operator sees
`key 'id' not found` with no file name, no array index, and no id. `config.id` is parsed first, so
wrapping the remainder of `ParseBuildingConfig` in a `catch`/rethrow that prepends
`"building '<id>'"` is cheap and would cover every field at once.

### [M] `IsCompleted` also answers true for projects that no longer exist
`src/game/buildings/SecretProjectAvailabilityCalculator.cpp:18-21` — a razed project is
tombstoned in `GameState` and reported as "completed" forever. That is the right *availability*
answer, but the method name and its header comment ("has been completed in any base of any
faction", `SecretProjectAvailabilityCalculator.h:19`) both promise something else. The first
caller that wants the honest question — a UI "owned by <faction>" label, a score or victory
check, diplomacy — will read this as ownership and be wrong. Rename to something like
`IsUnavailable`, or split the tombstone check from the ownership scan.

### [M] `BuildingConfig_t` has no default member initialisers
`include/game/buildings/BuildingConfigParser.h:20-26` — `category`, `mineralCost`, `allowMultiple`
and `bIsSecretProject` are uninitialised while `orbital` alone gets `= false`. The parser assigns
all of them so it is safe today, but the struct is default-constructed outside the parser:
`tests/faction/BuildingTechGateTests.cpp:28` and `:38` set only `id`/`requiredTech` and leave
`mineralCost` — which `GetBaseCost()` hands to the production system — indeterminate. Give every
member an initialiser; the project rule is that a constructed object is valid.

### [M] `BuildingRegistry` skips the validation extension point it inherits
`include/game/buildings/BuildingRegistry.h:9` — the class is an empty derivation (justified, since
an alias could not be forward-declared), but it never overrides `Validate_`, unlike `TechRegistry`
(`include/game/research/TechRegistry.h:16`), `PopTypeRegistry` and `CouncilProposalRegistry`. That
is the natural home for the whole-set checks buildings currently lack: `secret_project` combined
with `allow_multiple` is self-contradictory (once built, `IsCompleted` blocks it anyway, so the
flag is dead), and nothing rejects a negative `mineral_cost`. Instead, building validation is
spread over two free functions in `src/game/` that the composition root must remember to call.

### [M] The config struct lives in the parser header, so `nlohmann/json.hpp` leaks everywhere
`include/game/buildings/BuildingConfigParser.h:16-43` — `BuildingConfig_t` is defined next to the
parser, and the header includes `<nlohmann/json.hpp>`. Eighteen files include it, most of which
want only the data struct: `include/game/effects/ActiveEffect.h`, `include/game/faction/base/BaseManager.h`,
`include/game/orbital/OrbitalCensus.h`, `include/ui/satellite/SatelliteView.h`. The project already
splits these elsewhere (`UnitComponentConfig.h`, `SocialPolicyConfig.h`); a `BuildingConfig.h`
holding the struct would keep the JSON dependency inside the two parsing translation units.

### [L] Convention and hygiene items
- `include/game/buildings/BuildingConfigParser.h:23,26` — `allowMultiple` and `orbital` lack the mandated `b` prefix while the neighbouring `bIsSecretProject` has it.
- `src/game/buildings/BuildingConfigParser.cpp:11-13` and `include/game/buildings/BuildingConfigParser.h:48-49` — a user-provided empty constructor and a defaulted destructor on a class with no state; `ParseBuildingConfig` touches no member, so the whole class could be free functions in a namespace.
- `include/game/buildings/SecretProjectAvailabilityCalculator.h:19` — takes `const std::string&` where the rest of the building API uses the `BuildingId_t` alias.
- `src/game/buildings/SecretProjectAvailabilityCalculator.cpp:28` — dereferences `pBuilding` unchecked while every other loop over the same container null-checks it (`BuildingManager.cpp:59`, `OrbitalCensus.cpp:25`, `BaseConquestEffects.cpp:61`). The invariant does hold (entries come from `Registry::Get`); pick one policy — ideally state the invariant here and drop the defensive checks elsewhere.
- Test gap: the only coverage of this calculator is the tombstone case (`tests/game/BaseConquestTests.cpp:471`). Nothing asserts the primary rule — that a project completed in another faction's base disappears from `GetBuildingsAvailableForConstruction`.
- Prior review item 4.3 records `BuildingConfig_t`'s `IConstructable` inheritance and its embedded `IsAvailable` rule logic as deliberately deferred; not re-reported.

**Observed outside slice:**
- `src/game/units/ProbeActionEffects.cpp:114-128` — probe sabotage picks a random building excluding only the hardcoded `"Headquarters"`, so it can destroy a Secret Project without calling `MarkSecretProjectDestroyed`; the project then becomes buildable again, contradicting the raze tombstone at `src/game/units/BaseConquestEffects.cpp:106-111`.
- `src/game/faction/base/buildings/BuildingManager.cpp:24-31` — `AddBuilding` enforces nothing: not the tech gate, not `allowMultiple`, not secret-project uniqueness. It is the single point where all three could be enforced.
- `src/game/orbital/OrbitalAttack.cpp:40` and `src/game/units/InterceptRules.cpp:187` — same missing tombstone if an `orbital` building is ever also a Secret Project.
- `docs/architecture/high-level.md:270` — still says definitions load from `config/buildings.json`; it has been the directory `config/buildings/` since the multi-file loader landed.
- `config/buildings/README.md:17-24` — the field table omits the required `category`, and both worked examples (`:112`, `:129`) would fail to load as written.

---

## Planetary Council — runtime and outcomes

**Files:** `src/game/council/PlanetaryCouncil.cpp`, `include/game/council/PlanetaryCouncil.h`,
`src/game/council/CouncilOutcomeApplier.cpp`, `include/game/council/CouncilOutcomeApplier.h`,
`src/game/council/CouncilEffects.cpp`, `include/game/council/CouncilEffects.h`,
`src/game/council/CouncilAiStub.cpp`, `include/game/council/CouncilAiStub.h`

**Assessment:** The split into vote lifecycle / continuous-effect store / outward mutation is
genuinely good, the constructor enforces its membership invariants, and `Resolve` has a single
exit that clears `m_pending` before emitting `OnResolved` — the ordering hazards are handled.
The dominant weakness is the state model itself: the "state machine" is one
`std::optional<PendingProposal_t>` plus a `std::vector<std::string>` of active ids that means
two different things, with no way to leave a pending vote once entered. Vote tallying and the
election-candidate rule are also not where they claim to be.

### [H] `m_activeProposalIds` conflates "law in force" with "already enacted"
`src/game/council/PlanetaryCouncil.cpp:532-535` — every passed non-election proposal is pushed
into the active set, even one whose only content is a repeal or an instantaneous grant. Since
`CanPropose` rejects `IsActive(id) && !repeatable` (`:252`), a pure repeal is consumed for the
rest of the game: `repeal_un_charter` (`config/council/proposals.json:105`, no effects, not
repeatable) becomes permanently "active", so after `reinstate_un_charter` the charter can never
be repealed again — the exact flip-flop the code's own comment at `:257-260` says it wants to
support. The same conflation lets `requiredProposals` gates be satisfied by one-shots that are
not laws. Fix direction: keep the in-force set to proposals that actually contribute continuous
effects (`HasContinuousWorldEffects_` already exists), and drive repeatability from
`m_passCounts` plus a config field rather than from the marker.

### [H] A pending vote has no exit other than unanimous participation
`src/game/council/PlanetaryCouncil.cpp:592` — `Resolve` throws unless `AllMembersVoted()`, and
nothing can clear `m_pending` (no cancel, no timeout, no "absent counts as abstain" resolve),
while `Propose` throws whenever `m_pending` is set (`:276-279`). Any member that never casts a
ballot therefore bricks the council permanently. That is reachable in the shipped build today:
`CastStubCouncilVotes` has no caller outside `tests/game/PlanetaryCouncilTests.cpp` (verified by
search; `Engine.cpp` only creates the two views), so the AI members of a player-initiated vote
never vote, `CouncilVoteView::TryResolveAndClose_` silently returns, Escape closes the view with
`m_pending` still set, and `CommlinksView::OnProposalSelected_` then no-ops forever. Note
`TallyStandard_` already treats a missing ballot exactly like `Abstain` (`:459-462`), so the
strict gate buys nothing the tally needs. Fix direction: make participation a council concern —
resolve absentees as abstentions and/or drive ballots through an injected voter abstraction (see
below) — rather than leaving the terminal state reachable.

### [M] The AI stub is a free function, not a seam the council owns
`src/game/council/CouncilAiStub.cpp:10` — `CastStubCouncilVotes(PlanetaryCouncil&)` is a global
that reaches into the council from outside: it re-reads `GetPending()`, re-derives who may vote,
and needs `Members()` to hand out non-const `Faction*` from a const accessor
(`include/game/council/PlanetaryCouncil.h:58`). There is no `ICouncilVoter`-style interface, so a
real AI cannot be substituted or tested against the council — it can only be a second free
function with the same reach-in, and every caller (currently only tests) must know to invoke it.
It also hardcodes rules the council owns: `GovernorCandidates()` is used as the candidate pool
for *any* election including Supreme Leader (`:33`), and all AI members are given one shared
choice computed once (`:20-39`). Fix direction: define a narrow voter interface (`Decide(pending,
member) -> ballot`) that the council calls for non-player members when a proposal opens, and make
the stub the first implementation.

### [M] The "two most populous factions" governor rule is not enforced
`src/game/council/PlanetaryCouncil.cpp:353` — `CastElectionVote` only checks that the candidate is
a council member, so any member can be elected Planetary Governor. `GovernorCandidates()`
(`:178-205`) computes the top two but is consumed only by the AI stub and tests; the UI passes the
full membership as the candidate list (`src/ui/council/CouncilVoteView.cpp:73`). The rule stated in
the proposal description and in `docs/architecture/council-system.md` therefore lives in a function
nobody enforcing anything calls — two sources of truth, and the UI is the one that decides.
`CastElectionVote` should validate the candidate against the eligible set for the proposal's
`electionOutcome`.

### [M] `voteThreshold` is silently ignored for standard ballots
`src/game/council/PlanetaryCouncil.cpp:452-473` — `TallyStandard_` returns `yea > nay` and never
reads `rConfig.voteThreshold`, though the field is documented as "fraction of total vote weight
required for passage (elections / specials); 0 = simple majority"
(`include/game/council/CouncilProposalConfig.h:39-41`). A modder who sets `vote_threshold: 0.66`
on a standard proposal gets a plain majority with no error at load or at resolve. Either honor the
threshold in `TallyStandard_` or reject a non-zero threshold on `Standard` proposals.

### [M] A governor veto on an election can never be overruled
`src/game/council/PlanetaryCouncil.cpp:391` — `VetoUnanimouslyOverruled_` looks only at
`m_pending->ballots`, which is always empty for `Election` proposals (they populate
`electionVotes`). So the documented escape hatch ("every non-governor voted Yea") is structurally
unreachable for elections, giving the governor an absolute, unappealable veto over both governor
re-election and the Supreme Leader victory. If that asymmetry is intended it must be stated and
tested; otherwise the overrule test needs an election form (every non-governor voted for the same
candidate).

### [M] Tallying is not separable or testable
`src/game/council/PlanetaryCouncil.cpp:452`, `:476` — both tally helpers are private, take no
ballot argument, and dereference `m_pending` on the assumption that only `ResolveOutcome_` calls
them; the invariant is implied, not asserted. Testing "does 2 Yea beat 1 weighted Nay" currently
requires a `GameState`, a `WorldMap`, three `Faction`s and three bases (see the `CouncilGame_`
fixture). Extracting the tally as a free function over (members, weights, ballots, threshold)
would make the voting rules unit-testable and let `Resolve` stay a thin driver — which is what the
architecture doc already claims the split achieves.

### [M] The applier ignores per-effect targeting on instantaneous outcomes
`src/game/council/CouncilOutcomeApplier.cpp:23-35` — `GrantEnergy` is applied to every council
member unconditionally; `rEffect.factionFilter` and `rEffect.condition` are never consulted, so a
proposal that grants energy only to the proposer or only to signatories silently pays everyone.
Any other Instantaneous variant falls through both branches with no diagnostic. The scope/variant
inertness may be the project's deliberate "legal but inert" policy, but silently *widening* a
declared `factionFilter` is a different failure — it produces wrong values rather than none.

### [M] `ComputeVoteWeight` rebuilds the whole effect pool per member, per call
`src/game/council/PlanetaryCouncil.cpp:155-175` — each call copies the faction's entire cached
local effect pool, then appends copies of the governor and world effect entries, to read a single
`CouncilVotes` stat. `TallyStandard_`/`TallyElection_` call it once per member, and
`CouncilFactionVotesPanel::Render` calls it once per member *per frame*
(`src/ui/council/CouncilFactionVotesPanel.cpp:119`). Effects package 3 removed the by-value
`CollectWorldEffects()`/`CollectFactionEffects()` copies (both return `const&` now); what remains
is the whole-pool copy — filter by `StatId_t::CouncilVotes` first, and key the UI read on a
revision rather than recomputing per frame.

### [M] A missing member silently turns a won election into a failure
`src/game/council/PlanetaryCouncil.cpp:517` — `FindMember_(bestId)` can return null, and
`ResolveOutcome_` infers `passed = pElectionWinner != nullptr` (`:572`), so an unresolvable winner
id is reported as `Failed` instead of as the invariant violation it is. Per the project guideline
("throw errors if expected pointers are null") this should throw; as written the only signal is a
wrong outcome.

### [L] Convention and hygiene items
- `src/game/council/PlanetaryCouncil.cpp:367-376` — `VetoPending` returns `false` on precondition
  failure while every other entry point in the class throws; inconsistent, and the UI never checks.
- `src/game/council/PlanetaryCouncil.cpp:559-563` — `VetoUnanimouslyOverruled_()` is evaluated
  twice to compute two complementary booleans; one call into a local reads better.
- `src/game/council/PlanetaryCouncil.cpp:532-545` — activation is decided by a negated
  two-enumerator condition and then repeated positively in the governor branch; a single `switch`
  on `electionOutcome` would state the three cases once.
- `src/game/council/PlanetaryCouncil.cpp:200` — "top two" governor candidates is a hardcoded `2`;
  it belongs in `CouncilRulesConfig_t` with the propose intervals.
- `src/game/council/PlanetaryCouncil.cpp:180` — local data struct `Entry` should be `Entry_t` per
  the naming scheme; its comparator params `a`/`b` are references and want the `r` prefix.
- `src/game/council/PlanetaryCouncil.cpp:425-441` — `RemoveActiveProposal_`/`ActivateProposal_`
  each `RebuildWorld` and `Bump`, so one resolution with N repeals does N+1 full rebuilds and
  bumps the revision N+3 times.
- `src/game/council/CouncilOutcomeApplier.cpp:46` — `rMembers` is unused (commented out) in
  `ApplyGovernor`; drop the parameter rather than leave the signature lying. Neither applier
  method mutates the applier — both should be `const`.
- `src/game/council/CouncilEffects.cpp:39-51` — `SetGovernorEffects` rebuilds the world wrappers
  too, because `RebuildWrappers_` does both lanes unconditionally.
- `src/game/council/CouncilAiStub.cpp:25`, `:43` — `if (!pMember)` guards are dead: the
  constructor rejects null members.
- `docs/architecture/council-system.md` documents four lifecycle phases but never mentions
  `CouncilAiStub`, the one component whose replacement it should describe.

**Observed outside slice:**
`src/ui/council/CouncilVoteView.cpp:52` — `Resolve` is called from a UI callback with no
try/catch, so any council precondition throw becomes an unhandled exception from the render loop.

---

## Planetary Council — configuration and registries

**Files:** `src/game/council/CouncilProposalConfigParser.cpp`,
`include/game/council/CouncilProposalConfigParser.h`,
`include/game/council/CouncilProposalConfig.h`,
`include/game/council/CouncilProposalRegistry.h`,
`src/game/council/CouncilRulesConfigParser.cpp`,
`include/game/council/CouncilRulesConfigParser.h`,
`include/game/council/CouncilRulesConfig.h`

**Assessment:** Both parsers are small and readable, they throw on unknown enum strings rather
than defaulting, and the registry adds genuine cross-reference validation for
`required_proposals` / `repeals`. Cross-cutting references are also covered elsewhere:
`required_tech` by `RequiredTechValidator.cpp:78` and effect-borne ids by
`EffectReferenceValidator.cpp:194`, so those are not gaps. The dominant weakness is that the
proposal schema is a permissive *superset* of what the runtime honors — several field
combinations load cleanly and then do nothing at all, which is the one failure mode a
data-driven, moddable proposal list cannot afford. Adding a proposal built from existing
effect types is pure data (good); adding a new *outcome* still requires C++.

### [M] `kind` and `election_outcome` are parsed independently, and mismatches half-apply
`CouncilProposalConfigParser.cpp:53` and `:70-73` parse the two fields with no consistency
check. A `"kind": "standard"` proposal with `"election_outcome": "planetary_governor"` passes
its vote, records the pass, applies instantaneous effects — and is then neither activated
(`PlanetaryCouncil.cpp:532` skips activation for governor/supreme-leader outcomes) nor able to
install a governor (`:541` needs an election winner, which a standard ballot never produces).
The result is a proposal that reports `Passed` and changes nothing. The inverse — `election`
with no outcome — computes a winner in `TallyElection_` and discards it. Fix: reject the
mismatch at parse time (`electionOutcome != None` implies `kind == Election`), and state
explicitly in `CouncilProposalConfig.h` what an outcome-less election means.

### [M] `vote_threshold` is validated for every proposal but only honored by elections
`CouncilProposalConfigParser.cpp:55-60` accepts and range-checks `vote_threshold` regardless of
`kind`, which reads as a promise that it works. `TallyStandard_` (`PlanetaryCouncil.cpp:452-474`)
never reads it — a standard proposal always resolves on `yea > nay`, so a config author asking
for a two-thirds standard vote gets a simple majority with no warning. The field comment
compounds this: `CouncilProposalConfig.h:39-41` says `0 = simple majority of non-abstaining
weight (Yea > Nay)`, but on the only path that reads the field (`PlanetaryCouncil.cpp:505-517`)
`0` means *plurality* — any candidate with a single vote wins. Fix: reject `vote_threshold` on
`Standard` proposals until the tally honors it, and correct the comment to describe the
election semantics.

### [M] `vote_threshold` is a `double` where the project has an exact rational type
`CouncilProposalConfig.h:41` stores the threshold as `double` and
`CouncilProposalConfigParser.cpp:55` reads it as a JSON number, so the only way to express a
two-thirds supermajority is `0.6666...`. `PlanetaryCouncil.cpp:513-515` then compares it against
`bestVotes / totalWeight` computed in floating point, making the boundary case decided by
rounding. `lib/Rational.h` exists for precisely this ("config values that may be ints (2) or
fraction strings (\"1/3\")") and is already used by `ImprovementConfigParser.cpp:209`. Fix:
parse `vote_threshold` with `Rational_t::ParseJson` and have the tally compare cross-multiplied
integers.

### [M] Interval defaults live in two places and a misspelled key falls back silently
`CouncilRulesConfigParser.cpp:28-31` reads both intervals via `json.value(key, <struct default>)`,
so `governor_propose_interval_years` misspelled in `rules.json` yields `10` from
`CouncilRulesConfig.h:14` with no diagnostic, and the shipped `config/council/rules.json` repeats
the same two numbers that the header already hardcodes — two sources of truth that can drift.
The project guideline is to prefer throwing over returning defaults. Fix: require both keys (and
drop the header initializers), or keep the defaults and reject unknown keys in the object.

### [M] `required_proposals` documents an invariant the runtime does not hold
`CouncilProposalConfig.h:49-50` says the listed ids "must currently be in force (active)", but
`PlanetaryCouncil.cpp:535` adds *every* passed non-election proposal to the active set and never
removes it unless something repeals it. So "active" means "in force **or** has ever passed", and
the two shipped uses want different things: `increase_solar_shade` → `launch_solar_shade` means
"has passed" (that proposal projects nothing continuous), while `repeal_trade_pact` →
`global_trade_pact` really does mean "in force". A modder reading the comment cannot predict
which they get. Fix: either state the actual semantics on this field, or split it into
`requires_in_force` / `requires_passed` and let the council answer each from the right store
(`m_activeProposalIds` vs `m_passCounts`).

### [L] Convention and hygiene items
- `include/game/council/CouncilProposalConfigParser.h:17-20` — private methods lack the required
  trailing underscore (`ParseProposalConfig`, `ParseVoteWeight`, `ParseKind`,
  `ParseElectionOutcome`), while the anonymous-namespace free function
  `CouncilProposalConfigParser.cpp:15` carries one it does not need.
- `src/game/council/CouncilProposalConfigParser.cpp:80-92` — `ParseVoteWeight` / `ParseKind` are
  hand-rolled string chains whose wire forms differ from the enumerators only by case
  (`"standard"` ↔ `Standard`); the guidelines call for `magic_enum::enum_cast` here, as
  `EffectConfigParser.cpp:419` already does. `ParseElectionOutcome` correctly keeps an explicit
  map (`"supreme_leader"` ↔ `SupremeLeaderVictory`).
- `src/game/council/CouncilProposalConfigParser.cpp:80-101` — all three helpers are stateless
  `const` members; moving them beside `ParseRuleFlagList_` in the anonymous namespace would
  shrink the header to the single `ParseConfig` the registry template requires.
- `src/game/council/CouncilRulesConfigParser.cpp:14-25` — open / parse / `is_object` boilerplate
  duplicated in at least four other single-object parsers (`MoraleConfigParser.cpp:58`,
  `BaseConquestConfigParser.cpp:12`, `TileYieldRulesConfigParser.cpp:14`,
  `GrowthConfigParser.cpp:13`); `lib/config/JsonConfigLoader.h` only covers top-level arrays and
  wants an object variant.
- `src/game/council/CouncilRulesConfigParser.cpp:47-48` — the `wrapper["effects"] = ...` trick to
  satisfy `ParseEffects`' container contract is duplicated at `ProbeActionConfigParser.cpp:60`; a
  `ParseEffectList(const json& array)` overload in `EffectConfigParser` would remove both.
- `src/game/council/CouncilProposalConfigParser.cpp:17-33` — `ParseRuleFlagList_` re-implements the
  string-array read that `ConfigFields::ParseStringArray` already provides; only the flag mapping
  is council-specific.
- `include/game/council/CouncilProposalConfig.h:59-68` — `IsAvailable` is the fourth verbatim copy
  of the same linear discovered-tech scan (`BuildingConfigParser.h:36`, `SocialPolicyConfig.h:29`,
  `PopTypeAvailabilityCalculator.cpp:29`).
- `src/game/council/CouncilProposalConfigParser.cpp:47`, `CouncilRulesConfigParser.cpp:12` —
  reference parameters `proposalJson` / `configPath` lack the `r` prefix used by `rJson` and
  `rValue` in the same files.

**Observed outside slice:**
- `tests/fixtures/council/proposals.json`, `tests/fixtures/council/rules.json` — byte-identical
  copies of the shipped `config/council/*` files; the suite will keep passing after the real
  config drifts away from them.
- `src/game/council/PlanetaryCouncil.cpp:535` — every passed non-election proposal is activated
  permanently, so `repeal_trade_pact` (not `repeatable`, no continuous effects) can be used once
  per game even though the pact it repeals can be re-enacted; same for `repeal_un_charter`.
- `src/game/council/CouncilOutcomeApplier.cpp:31-34` — `GrantEnergy` is paid to every member
  regardless of the effect's declared `scope` or any faction filter, so those fields are inert
  decoration on instantaneous proposal effects.

---

## Faction — economy, research, social engineering, identity

**Files:** `{src/game,include/game}/faction/` — `EconomyManager.{cpp,h}`,
`ResearchManager.{cpp,h}`, `ResearchSelector.{cpp,h}`, `SocialEngineeringManager.{cpp,h}`,
`FactionEffectsPool.{cpp,h}`, `Specialist.{cpp,h}`, `AIProfile.{cpp,h}`,
`FactionConfigParser.{cpp,h}`, `FactionFlavor.{cpp,h}`, `FactionIdentity.{cpp,h}`,
`FactionConfig.h`, `FactionRegistry.h`

**Assessment:** `FactionEffectsPool` is the strongest piece here and its **invalidation is
complete**: I traced every mutator of every contributor — `BuildingManager::AddBuilding` /
`DestroyBuilding`, `PopContainer::AddPop` / `RemovePop` / `ConvertTo` / `ConvertToFallback`,
`UnitManager::CreateUnit` / `DestroyUnit`, `SocialEngineeringManager::SetActivePolicy`,
`Faction::AddBase` / `ExtractBase`, `ResearchManager::AddDiscoveredTech` — and each bumps a
`Revision` that `CollectRevisions_` stamps. `Unit`'s design is a `const&` member so unit
effects cannot change in place, and `UnitManager::Units()` filters the null slots left by
deferred destruction, so a rebuild during a `DeferredDestructionScope` is safe. The defects
are elsewhere: `Rebuild_` interleaves *expansion* passes with *collection* passes so two
kinds of effect are consumed before their producers exist or after their gate is lifted, and
the cache is not bound to the faction it was built for. Outside the pool, the dominant
weakness is optional-pointer dependencies handled three different ways per class, plus a
research cost cache keyed on only one of its two inputs. `EconomyManager`, `FactionIdentity`,
`AIProfile` and `Specialist` are thin to the point of being liabilities.

### [H] `EconomyManager` owns the treasury but offers no way to spend from it
`src/game/faction/EconomyManager.cpp:12-15` — `AddEnergy` is the only mutator and applies
any signed amount unchecked, so spending is written as `AddEnergy(-cost)` at four call sites
(`src/game/units/ProbeActionExecutor.cpp:152`, `src/game/units/UnitOrderExecutor.cpp:364`,
`src/game/units/ProbeActionEffects.cpp:99`, `src/game/faction/DiplomaticActionExecutor.cpp:314`)
and the "can I afford it" rule is re-implemented at three more
(`ProbeActionExecutor.cpp:148`, `src/game/units/TerraformRules.cpp:161`,
`DiplomaticActionExecutor.cpp:226`). All four sites currently check, so nothing is broken
today, but the invariant "the treasury never goes negative" lives in the callers rather than
in the class that owns the resource, and the fifth spender will be the one that forgets —
with no bankruptcy rule, the result is a silently negative treasury. A `SpendEnergy(int)`
that throws (or a `CanAfford`/`TrySpend` pair) would make the rule unforgeable and delete
three duplicated checks.

### [M] Optional dependencies with three different null policies per class
The four constructor pointers of `FactionEffectsPool` are all nullable, two with `= nullptr`
defaults (`FactionEffectsPool.h:23-26`), and each is handled by silently skipping work — a
null building registry returns the building effects **unexpanded**
(`FactionEffectsPool.cpp:52-55`), so every `GrantBuilding` chain vanishes with no diagnostic.
The same pointer inside one class is treated
three different ways elsewhere: `ResearchManager` dereferences `m_pTechRegistry` unchecked in
`SetResearchTarget` (`ResearchManager.cpp:32`), throws on it in `RecalculatePointsNeeded`
(`:107-110`), and returns `{}` for it in `GetAvailableTechs` (`:196-199`);
`ResearchSelector` throws on a null manager in `GetCandidateTargets`
(`ResearchSelector.cpp:52-56`), returns `false` in `AssignResearchTarget` (`:88-91`) and
returns silently in `EnsureResearchTarget` (`:110-113`). The guidelines call for constructors
that produce valid objects and for throwing on unexpected null; the single composition root
(`src/game/Faction.cpp:50-58`) always supplies every one of these, so they can all become
references.

### [M] Hardcoded default policy ids now hard-fail instead of silently failing
`src/game/faction/SocialEngineeringManager.cpp:16-19,42-52` — the four starting policies are
still compiled-in string literals (`"frontier"`, `"simple"`, `"survival"`, `"none_future"`).
This is the fix recorded for prior finding 1.11, and it is incomplete in the direction the
guidelines care about: validating the hardcoded ids converts "mod ships different starting
policies → `GetActivePolicy` returns nullptr forever" into "mod ships different starting
policies → every faction constructor throws and the game will not start". The ids belong in
config (a `default: true` flag per category in `social_policies.json`, or a
`starting_policies` block), validated the same way. The `if (!m_pRegistry) return;` at
`:30-33` also skips the whole check, leaving a manager with no policies at all — a fourth
null policy in a class that throws on a null registry two methods later (`:61-64`).

### [M] `GetSocialRating` recollects and re-accumulates the whole rating map per query
`src/game/faction/SocialEngineeringManager.cpp:100-105` — every call runs `CollectEffects()`
(a fresh vector with a `std::string sourceId` per effect) and `AccumulateSocialRatings` (a
fresh `std::map`) to answer a question about *one* axis. `SocialEngineeringDisplay` calls it
once per rating axis per frame (`src/ui/social-engineering/SocialEngineeringDisplay.cpp:120,412`),
so this is ten vector+map allocations per frame — the one un-memoized read left on the SE
side after the pool work. The class already owns the `Revision` it would need to cache the
accumulated map against.

### [M] `FactionIdentity` is a bypassed duplicate of the config it copies
`src/game/faction/FactionIdentity.cpp:6-13` copies six strings out of `FactionIdentityConfig`
and `LeaderConfig` into a heap-allocated object (`src/game/Faction.cpp:45`) whose only
consumer in the whole codebase is `FactionFlavor` — `Faction` exposes no `GetIdentity()` at
all. Every other reader goes to the config directly: eleven sites use
`GetDefinition().identity.name` (e.g. `src/game/council/CouncilFactionVotesPanel.cpp:52,117`,
`src/ui/commlinks/CommlinksPanel.cpp:52`), and the two identity fields that carry real rules —
`participatesInCouncil` and `species` — are not on `FactionIdentity` at all, so
`src/game/units/BaseConquestEffects.cpp:292` and `src/game/council/PlanetaryCouncil.cpp:46`
have to bypass it. The result is two representations of faction identity where the
authoritative one is the config. Give `FactionFlavor` the `FactionIdentityConfig` and
`LeaderConfig` directly and delete the class.

### [M] Flavor RNG cannot be seeded, so base names are not reproducible
`src/game/faction/FactionFlavor.cpp:31` — the only constructor seeds `m_rng` from
`std::random_device`, and unlike `ResearchSelector` (which offers
`ResearchSelector(pManager, uint32_t seed)`, `ResearchSelector.cpp:34-39`) there is no seeded
overload. Base names are persistent game state, so this makes a run unreproducible from its
inputs and leaves `tests/faction/FactionFlavorTests.cpp` unable to assert which name is
picked. Two RNG-seeding policies inside one slice is also a coin-flip for the next
subsystem; a single game-level seed source would settle both.

### [L] Convention and hygiene items
- `include/game/faction/FactionConfig.h:19,33,38,46` — `FactionIdentityConfig`,
  `LeaderConfig`, `AITendenciesConfig`, `FactionFlavorConfig` are data structs and need the
  `_t` suffix (`FactionConfig_t` and `EnergyAllocation_t` in the same slice have it).
- `include/game/faction/Specialist.h` / `src/game/faction/Specialist.cpp` — a header with
  nothing but a comment and a `.cpp` that includes only itself, compiled into `ac-core`
  (`src/CMakeLists.txt:55`). Nothing includes the header. Delete both.
- `src/game/faction/AIProfile.cpp` — `AIProfile`'s four `InterestedIn*` getters have zero
  callers and `Faction::m_pAIProfile` is never read; the guideline is "no getters without an
  immediate requirement".
- `src/game/faction/FactionConfigParser.cpp:26-31` — `ParseFactionSpecies_` is a hand-rolled
  string→enum map living in the parser; the guideline puts the one map next to the enum
  (`FactionConfig.h:12-17`), and `magic_enum` is already used elsewhere in the project.
- `src/game/faction/FactionConfigParser.cpp:168-188` — `ReadJsonFile` has exactly one caller,
  `ReadRequiredJsonFile`, whose `fs::exists` check duplicates the `is_open` failure below it.
  Also `std::cout` progress logging in a parser at `:39,70`.
- `src/game/faction/ResearchSelector.cpp:14-25` — `CategoryIndex_` is a second source of
  truth for category ordering next to `k_AllGameCategories` (`include/game/GameCategory.h:21`);
  `magic_enum::enum_index` removes it.
- `src/game/faction/ResearchSelector.cpp:66-70,99-103` — two dead guards:
  `ResearchManager::GetAvailableTechs` only ever pushes `&rConfig` (`ResearchManager.cpp:223`)
  and `PickRandom_` cannot return null.
- `src/game/faction/ResearchManager.cpp:230-233` — `ResetAccumulatedPoints_` is declared,
  defined and never called.
- `src/game/faction/ResearchManager.cpp:172-182` — `TechId` is `std::string`;
  `HasDiscoveredTech(TechId)` takes it by value and the range-`for` copies every element it
  scans. Called per prerequisite in `GetAvailableTechs` and per effect in the pool's
  `removed_by_tech` filter. Same for `SetResearchTarget` and `AddDiscoveredTech`.
- `include/game/faction/FactionEffectsPool.h:33` — "changes iff the pool content changed" is
  not true: any contributor bump rebuilds and increments the version even when the content is
  identical (add then destroy an effect-less building), needlessly invalidating
  `BaseManager::BuildBaseEffects_` and the research cost.
- `include/game/faction/SocialEngineeringManager.h:25-26` — the comment says `SetActivePolicy`
  throws "when a registry is bound"; it now also throws when one is not
  (`SocialEngineeringManager.cpp:61-64`). It also takes a whole `SocialPolicyConfig_t` only to
  read `.id` and look it up again — the id is the narrower parameter.
- `include/game/faction/FactionFlavor.h:24-25` — two reference members with no stated
  lifetime requirement; they alias registry-owned config that must outlive the faction.
- `src/game/faction/FactionFlavor.cpp:56,76-81` — the fallback base name (`"<Adjective> Base
  N"`) and the six substitution tokens are compiled-in English.
- Out-of-line empty/defaulted special members that could be `= default` in the header:
  `EconomyManager.cpp:8-10`, `ResearchManager.cpp:26-28`, `AIProfile.cpp:6-17`,
  `FactionIdentity.cpp:16-18`, `SocialEngineeringManager.cpp:55-57`.
- Reference parameters missing the `r` prefix throughout
  `FactionConfigParser.cpp` (`j`, `configPath`, `dirPath`, `filePath`) and
  `FactionFlavor.h:20` (`category`).
- `include/game/faction/FactionRegistry.h:10` — an empty subclass of `Registry<>` that exists
  only so `GameDataContext.h:19` can forward-declare it; needs a comment saying so.

**Observed outside slice:**
- `include/game/faction/base/population/PopContainer.h:33` + `include/game/population/pop-types/Pop.h:56` — `Pops()` hands out mutable `Pop&` and `Pop::Convert` is public, so a pop's type (a pool input) can be changed without bumping `PopContainer::m_revision`; only `PopContainer` calls it today, so the "every mutator bumps" invariant holds by discipline alone.
- `src/game/faction/base/BaseManager.cpp:311-324` — `BuildBaseEffects_()` calls `GetActiveEffects()` and `GetEffectsVersion()` back to back, validating the pool (a full per-base revision traversal) twice per stat read.
- `docs/architecture/faction-system.md:31-39,205-231` — the economy/AI subsystems are documented with members that do not exist (`Credits`, `TradeRoutes`, `IncomeCalculator`, `Personality`, `Priorities`, `UnitFactory`), and `FactionEffectsPool`, `ResearchSelector`, `SocialEngineeringManager` and `UnitManager` are absent entirely; `docs/architecture/high-level.md:285` lists the same stale subsystem set.

---

## Faction — military, units, diplomacy, visibility

**Files:** `src/game/faction/Military.cpp`, `include/game/faction/Military.h`,
`src/game/faction/UnitManager.cpp`, `include/game/faction/UnitManager.h`,
`src/game/faction/UnitVisibility.cpp`, `include/game/faction/UnitVisibility.h`,
`src/game/faction/VisibilityRules.cpp`, `include/game/faction/VisibilityRules.h`,
`src/game/faction/FactionVisibleMap.cpp`, `include/game/faction/FactionVisibleMap.h`,
`include/game/faction/FactionExploredMap.h`, `include/game/faction/FactionRevealedUnits.h`,
`src/game/faction/DiplomacyActions.cpp`, `include/game/faction/DiplomacyActions.h`,
`src/game/faction/DiplomacyLedger.cpp`, `include/game/faction/DiplomacyLedger.h`,
`src/game/faction/DiplomaticActionExecutor.cpp`, `include/game/faction/DiplomaticActionExecutor.h`,
`src/game/faction/FirstContactResolver.cpp`, `include/game/faction/FirstContactResolver.h`,
`include/game/faction/FactionPair.h`, `src/game/faction/TradeItem.cpp`,
`include/game/faction/TradeItem.h`

**Assessment:** The two questions this slice is most often asked, it answers well. Unit
ownership has exactly one owner — `UnitManager::m_units` — with no leaked owning container
(`Units()` is a reference view) and no second position store: `Unit::m_pTile` is written
only by `UnitPositionIndex`, which is `friend`-gated. Diplomatic state cannot desync,
because `DiplomacyLedger` keys every symmetric fact on `FactionPair::Canonical` and every
asymmetric fact on `DirectedFactionPair`, so there is only one cell per fact. The three
visibility structures are likewise a clean split, not overlapping caches: explored
(monotonic memory), visible (derived, cleared and rebuilt), revealed-units (contact memory
keyed by stable id). The dominant weakness is *when* that derived state is recomputed — a
whole-map scan fires on every unit event — and the diplomacy executor, which validates a
proposal item-by-item but applies it as a whole, and reaches its collaborator through a
post-construction setter.

### [H] Visibility rebuild is a whole-map, per-event recompute
`src/game/faction/FactionVisibleMap.cpp:104` — `RebuildFromSources` clears and re-derives
the visible map by walking *every tile of the world*, and for each improvement on each tile
calling `SightRadiusFromImprovement_` (`FactionVisibleMap.cpp:32`), which allocates a
`std::vector<ActiveEffect_t>` and runs `ResolveStatModifiers` per candidate effect. That
result depends only on static `ImprovementConfig_t` data, so all of it is recomputed for
nothing. The trigger frequency makes it worse: `UnitManager::CreateUnit` (`UnitManager.cpp:66`)
and `DestroyUnit` (`UnitManager.cpp:117`) each rebuild, and `DestroyUnit` recurses per
passenger, so sinking a loaded transport does one full-map scan per unit aboard; every move
rebuilds via `GameState`'s `OnUnitMoved`; and each rebuild then invokes
`FirstContactResolver::ConsiderObserver` (`FirstContactResolver.cpp:62`), which scans every
other faction's units and bases. Memoize the per-config sight radius and keep a
vision-improvement tile list on the world (or gate the rebuild behind a dirty flag /
deferral scope like `DeferredDestructionScope`) before map or unit counts grow.

### [H] `DestroyUnit` is also the transfer path, so transfers apply combat cargo rules
`src/game/faction/UnitManager.cpp:77` — `DestroyUnit` implements "carrier is lost": cargo
that cannot hold the tile alone is destroyed, the rest is disembarked. `Faction::ExtractUnit`
(`src/game/Faction.cpp:357`) calls the same method to detach a unit for transfer, so
`TransferUnitTo` on a loaded transport silently drowns or strands its passengers, and the
receiving faction gets an empty hull. The reverse case is worse: extracting an *embarked*
unit clears its carrier link, and the re-create then runs
`CanPlaceUnitOnTile` (`UnitManager.cpp:54`) against a tile the carrier still occupies —
under `SetSingleUnitPerTile(true)` that throws *after* the source unit is gone, losing it.
`UnitManager` needs a "release ownership without applying destruction rules" operation
distinct from `DestroyUnit`.

### [H] Trade items are validated one at a time and applied all at once
`src/game/faction/DiplomaticActionExecutor.cpp:227` — each `TradeCredits_t` is checked
against the giver's *full* treasury independently, then `ApplyItems_` debits them all
(`DiplomaticActionExecutor.cpp:314`). Two credit items of the whole balance both validate
and both apply; `EconomyManager::AddEnergy` (`src/game/faction/EconomyManager.cpp:12`) has
no floor, so the giver ends the trade with a negative treasury and no error anywhere. The
same shape will hit every future consumable item type. Validate a proposal's aggregate cost
per giver, or debit through a checked operation that throws when it would go negative.

### [M] The executor is two-phase-initialised and its dependency is a nullable pointer
`include/game/faction/DiplomaticActionExecutor.h:27` — `SetGameDataContext` is a
post-construction setter for a hard dependency; `ApplyItem_` throws
"GameDataContext not set" mid-apply (`DiplomaticActionExecutor.cpp:323`) *after* earlier
items in the same proposal have already been applied, leaving a half-executed trade. This
is a fresh instance of prior finding 4.2 (recorded as fixed for every case it listed):
take `const GameDataContext&` in the constructor and hold it as a reference.

### [M] One global pending-proposal slot, silently overwritten
`src/game/faction/DiplomaticActionExecutor.cpp:105` — `Propose` to a player-controlled
faction assigns `m_pending` unconditionally. A second proposal in the same turn discards
the first, yet its proposer already received `PendingPlayer` and will wait forever. `Accept`
also does not identify who is accepting. A per-recipient queue (or at minimum rejecting a
proposal while one is pending) is needed before any AI diplomacy drives this.

### [M] Unsized visible map silently means "sees everything"
`src/game/faction/UnitVisibility.cpp:97` — `if (rObserver.GetVisibleMap().IsSized() && !...IsVisible(rTile))`.
A faction whose `BindWorldMap` was never called sees every enemy unit on the map instead of
failing. `Faction::RebuildVisibility` has the mirrored silent no-op on a null world map.
This is a wiring error masked as a game rule; it should throw, with fixtures binding a map.

### [M] `TradeKind` and its probe table duplicate the `TradeItem_t` variant three times
`include/game/faction/DiplomacyActions.h:23` and `src/game/faction/DiplomacyActions.cpp:106`
— adding an alternative to `TradeItem_t` requires editing the enum, the `probes` array, the
parallel `kindOrder` array, and `ToString`, and nothing fails to compile if you miss one.
The probe table also depends on an unenforced invariant stated only in a comment ("CanTrade
only gates on relationship") — it passes `TradeCommFrequency_t{0}` and `TradeBase_t{1}`,
which become wrong answers the moment `CanTrade` inspects a payload. Derive the category
list from the variant (`std::variant_size` + a per-alternative `kind` trait) instead.

### [M] `Military` leaks its owning container and swallows a null design
`include/game/faction/Military.h:18` — `GetDesigns()` returns
`const std::vector<std::unique_ptr<UnitDesign>>&`, the exact pattern the prior review closed
for `UnitManager::GetUnits()` ("last leaked owning container", finding 1.1). `DesignListPanel`
consumes it and must dereference the smart pointers itself. `AddDesign`
(`src/game/faction/Military.cpp:12`) also returns `false` for both "duplicate id" and "null
pointer", so callers that report "duplicate" (`src/game/Engine.cpp:232`,
`src/ui/unit-designer/UnitDesignerView.cpp:160`) will misreport a null. Expose a `DerefView`
range and throw on null per the guidelines.

### [M] `FirstContactResolver` depends on GameState's concrete ownership container
`include/game/faction/FirstContactResolver.h:19` — it stores
`std::vector<std::unique_ptr<Faction>>&`, which is why the implementation carries `if
(!pOther)` / `if (!pObserver)` guards (`FirstContactResolver.cpp:68`, `:91`) for a condition
`GameState::AddFaction` already rejects. Everywhere else this container is hidden behind
`GameState::Factions()` (a `DerefView`); taking that range here removes the null branches
and the coupling to how factions happen to be stored.

### [L] Convention and hygiene items
- Enum classes missing the mandatory `_t` suffix: `DiplomaticStatus`
  (`include/game/faction/DiplomacyLedger.h:14`), `DiplomaticActionKind` and `TradeKind`
  (`include/game/faction/DiplomacyActions.h:12`, `:23`), `DiplomaticProposeResult`
  (`include/game/faction/DiplomaticActionExecutor.h:12`).
- `ToString(DiplomaticStatus)` (`src/game/faction/DiplomacyLedger.cpp:6`) is a hand-rolled
  switch whose strings match the enumerator names exactly — `magic_enum` per the guidelines,
  with only the `None → ""` case special.
- `DiplomaticActionExecutor.cpp:20-42` reimplements `FindFaction_` (both const overloads)
  when `GameState::FindFaction` (`include/game/GameState.h:81-82`) already exists.
- `std::map<FactionPair, bool> m_known` and `std::map<DirectedFactionPair, bool>
  m_infiltration` (`include/game/faction/DiplomacyLedger.h:59-61`) never store `false` — the
  setters erase instead — so the mapped value is dead weight; `std::set` states the invariant.
- Dead / speculative API in `include/game/faction/FactionRevealedUnits.h`: `Clear()` (:20),
  `GetRevision()` (:52), and the `UnitId_t` overloads of `Reveal`/`Forget`/`IsRevealed` have
  no callers anywhere, production or test. Same for `FactionVisibleMap::IsRemoveFog()`,
  `GetRevision()`, `GetWidth()`, `GetHeight()` (`FactionVisibleMap.h:30-35`, `:49`).
- `GetAvailableActions` / `GetAvailableTrades` / both `ToString` overloads in
  `DiplomacyActions.h:47-57` have no production caller — only tests.
- `src/game/faction/VisibilityRules.cpp:35` calls `MarkAll()` (a W×H write) after
  `SetRemoveFog(true)`, but `FactionVisibleMap::IsVisible` already short-circuits on the
  flag; the fill is redundant and runs on every rebuild.
- `FirstContactResolver::MeetIfNeeded_` (`FirstContactResolver.cpp:16`) re-checks `AreKnown`
  that both callers already checked; `ConsiderObserver` takes `Faction&` but never mutates it.
- `src/game/faction/TradeItem.cpp:3` includes `<sstream>` and never uses it;
  `include/game/faction/Military.h:19` uses `std::string` without including `<string>`.

**Observed outside slice:**
- `src/game/units/MovementRules.cpp:142` — `CanPlaceUnitOnTile` counts embarked cargo as
  occupying the tile, while `StepEvaluator.cpp:233` explicitly skips embarked units under
  the same stacking rule; the two readings of one rule disagree.
- `src/game/map/TileFlagMap.h:39` — `MergeFrom` is a silent no-op when sizes differ, so a
  `TradeWorldMap_t` between factions with mismatched maps would appear to succeed.

---

## Base management — BaseManager, home-base index, buildings

**Files:** `src/game/faction/base/BaseManager.cpp`, `include/game/faction/base/BaseManager.h`,
`include/game/faction/base/BaseTypes.h`, `src/game/faction/base/HomeBaseIndex.cpp`,
`include/game/faction/base/HomeBaseIndex.h`,
`src/game/faction/base/buildings/BuildingManager.cpp`,
`include/game/faction/base/buildings/BuildingManager.h`

**Assessment:** `BaseManager` is more than a pass-through: it owns the one genuinely
cross-subsystem job in the base (resolving the faction effect pool down to *this* base's
effect list) plus lifetime/identity, and the 2026-07 pass really did strip the population
and building pass-throughs. `HomeBaseIndex` is the strongest piece here — the claim *is*
the home-base link, so there is no unit-side field to desync, and destruction orphans
claims instead of dangling. The dominant weakness is that the base's effect list has **two
independent construction paths** that no longer produce the same list, and the memoized one
is the one the UI and half the turn pipeline read. Secondary: the constructor still accepts
ten nullable dependencies whose null behaviour is inconsistent (throw / silent skip /
deferred throw from a sub-manager).

### [H] The constructor accepts ten nullable dependencies with three different null behaviours
`src/game/faction/base/BaseManager.cpp:48`–`64` — every registry, calculator, config and
manager arrives as a raw pointer that may be null, and the class then reacts differently to
each: `BuildBaseEffects_()` throws (`:302`), `m_pSocialRatings == nullptr` **silently skips
all social-rating expansion** (`:292`), a null pop-composition calculator makes
`RecalculateComposition` a no-op, and a null research/secret-project calculator surfaces as a
throw from inside `BuildingManager::GetBuildingsAvailableForConstruction`
(`src/game/faction/base/buildings/BuildingManager.cpp:71`) at whatever call site happens to
ask for the build list. `BuildingManager`'s own constructor repeats the pattern
(`BuildingManager.h:22`–`25`). This is not hypothetical harness-only slack: `tests/GameFixtures.h:115`
builds bases with a null rating registry and null secret-project calculator, so fixture bases
resolve social ratings to nothing while the real game resolves them — a behaviour difference
with no diagnostic. Prior finding 4.2 records this as fixed with "the one deliberately
nullable dependency left is `pEffectsProvider`"; the signature says otherwise, so the
recorded fix is incomplete. Fix direction: take references for everything that must exist
and let the one genuinely optional dependency stay a documented pointer.

### [M] `BaseSnapshot_t` carries an untyped production id that only round-trips for buildings
`include/game/faction/base/BaseManager.h:44`–`55` and `BaseManager.cpp:137`–`140` store the
current production as a bare `std::string`, and reconstruction resolves it exclusively
through the building registry (`src/game/Faction.cpp:300`–`309`). `UnitDesign` is already an
`IConstructable` (`include/game/units/UnitDesign.h:16`) and `GetConstructable()` is typed
`const IConstructable*`, so the first base that can queue a unit will throw out of
`BuildingRegistry::Get` the moment it is captured — the transfer path fails, not the unit
feature. The snapshot also silently drops `ResourceManager`'s accumulated per-turn stockpiles
(`ResourceManager.h:59`–`63`), which is undocumented either way. Fix: capture the item's kind
alongside its id (or a typed handle), and state in the struct comment what deliberately does
not survive a transfer.

### [M] `HomeBaseIndex` keeps three representations of one relation
`include/game/faction/base/HomeBaseIndex.h:78`–`79` holds `m_claims` and `m_units` as
index-parallel vectors, and each claim additionally stores `m_pUnit` (`:42`). `m_pUnit` is
written in five places (`HomeBaseIndex.cpp:12`, `:46`, `:52`, `:62`, `:68`) and **read nowhere** —
release and move both locate entries by claim address. Two of the three copies are dead
weight that a future maintainer must keep consistent by hand (the `m_units.begin() + idx`
arithmetic at `HomeBaseIndex.cpp:104` is the only thing keeping the arrays aligned). Fix:
delete `m_pUnit`; keep `m_units` since `GetUnits()` returns it by reference.

### [M] `HomeBaseIndex` throws invariant violations from `noexcept` paths
`src/game/faction/base/HomeBaseIndex.cpp:100` throws `std::logic_error` from `Release_`,
reached from `~HomeBaseClaim` (`:18`, implicitly `noexcept`), and `:113` throws from
`UpdateClaimPointer_`, reached from `MoveFrom_` via the explicitly `noexcept` move
constructor/assignment (`HomeBaseIndex.h:25`–`26`). Either throw is an unconditional
`std::terminate`, not the catchable error the code appears to promise. I could not construct
a call sequence that reaches them today (claims are only created by `Claim`, which registers
them, and C++17 guaranteed elision makes the returned object the registered one), so this is
a contract problem rather than a live defect. Fix: make them assertions, or handle the
missing entry, and stop implying an exception contract that cannot hold.

### [L] Convention and hygiene items
- `include/game/faction/base/BaseManager.h:11` — `<functional>` is included but no
  `std::function` appears in the header.
- `include/game/faction/base/BaseManager.h:3` — `BuildingConfigParser.h` is included but no
  building config type is named in the header (`BuildingRegistry` is forward-declared);
  every consumer of a base pulls in the parser.
- `include/game/faction/base/BaseManager.h:40` — `using BaseId_t = int;` lives here while its
  sibling `FactionId_t` lives in `BaseTypes.h`, and the class then bypasses its own alias
  (`int m_baseId`, `int GetBaseId()` at `:190`).
- `include/game/faction/base/BaseTypes.h:12` and `:37` — `TradeRoute_t` and `TileCoord_t`
  have zero users anywhere in the tree; speculative types in a shared header.
- `include/game/faction/base/BaseTypes.h:19`–`24` — `TileResources_t`'s three ints have no
  default member initializers, so a default-constructed one is indeterminate.
- `src/game/faction/base/BaseManager.cpp:109`–`112` — dead null check: the preceding line's
  `AddBuilding` already throws on the same registry pointer.
- `src/game/faction/base/BaseManager.cpp:132` and
  `src/game/faction/base/buildings/BuildingManager.cpp:59` — null guards on a vector that
  cannot contain null (`AddBuilding` stores `&Registry::Get(...)`), while
  `DestroyBuilding`/`DoesBuildingExist_` dereference the same pointers unguarded.
- `src/game/faction/base/BaseManager.cpp:189`–`192` — `UserAssignBestAvailableWorker` is a
  one-line delegate to an already-public manager, contradicting the rule stated at
  `BaseManager.h:58`–`60` ("a method lives here only when it coordinates two or more
  subsystems").
- `src/game/faction/base/BaseManager.cpp:89` and `:120` — the `"Base"` improvement id is
  hardcoded in two places (prior review's recurring theme 2, magic config ids).
- `include/game/faction/base/BaseManager.h:206`,`:211` — two overloads named
  `BuildBaseEffects_` with materially different semantics (fresh vs memoized, supplied pool
  vs provider pool); the arity is the only cue at the call site.
- `src/game/faction/base/buildings/BuildingManager.cpp:20`–`22` — empty user-declared
  destructor (the class holds no incomplete types); it also suppresses implicit moves.

**Observed outside slice:**
- `docs/architecture/effects-system.md:462` — states `ResourceManager::ProduceResources()`
  "stores the effects"; it takes them per call and stores nothing (`ResourceManager.h:50`).

---

## Base management — population and production

**Files:** `src/game/faction/base/population/PopContainer.cpp`, `include/game/faction/base/population/PopContainer.h`, `src/game/faction/base/population/PopulationManager.cpp`, `include/game/faction/base/population/PopulationManager.h`, `src/game/faction/base/production/ProductionManager.cpp`, `include/game/faction/base/production/ProductionManager.h`, `src/game/faction/base/production/ProductionCostCalculator.cpp`, `include/game/faction/base/production/ProductionCostCalculator.h`

**Assessment:** Pop *identity* is genuinely well built: `vector<unique_ptr<Pop>>` + `DerefView` gives stable addresses under growth, the `WorkedTileClaim` RAII makes tile release automatic on conversion or death, and the `Revision` counter is bumped in every mutator. The dominant weakness is that the storage/rules split is only nominal — `PopContainer` owns the composition algorithm and pulls in `ResearchManager`, while `PopulationManager` is a pass-through for half its surface — and that both classes take every dependency as an optional pointer, so an invalid object is constructible and one of those pointers is dereferenced unchecked. `ProductionManager` is small and readable but has essentially no contract: it accepts any item, never validates, and silently drops both surplus minerals and invalid calls.

### [H] `PopContainer` owns composition policy and rules services, not storage
`src/game/faction/base/population/PopContainer.cpp:111-166` — `ApplyCompositionTargets` is the entire reconciliation *algorithm* (which pops change, in what order, four near-identical loops), and `ConvertToFallback` (`:90-109`) resolves the obsolescence chain through `PopTypeAvailabilityCalculator` + `ResearchManager`. Both are rules; both live in the class documented as the container. `PopulationManager` correspondingly degenerates into pure delegation (`PopulationManager.h:41-51`, `PopulationManager.cpp:38-41,73-77`). The concrete cost is inconsistent rule enforcement: `ConvertTo` (`PopContainer.cpp:75-88`) applies **no** tech/availability check even though the container holds the availability calculator, so `ConvertTo(rPop, "Thinker")` installs a pop type that `ConvertToFallback` would refuse — the tech gate exists on one conversion path only. Direction: leave the container with add/remove/convert-to-a-`PopTypeConfig_t&`, counts, and the revision, and move target reconciliation plus fallback resolution up into `PopulationManager`, which already computes the targets and can enforce availability once for every path.

### [H] Every dependency is an optional pointer, and one is dereferenced unchecked
`src/game/faction/base/population/PopulationManager.cpp:27` seeds `m_maxSize(pGrowthConfig ? pGrowthConfig->maxBaseSize : 7)` — declaring the growth config optional — yet `GetNutrientsRequired` (`:148`) and `ApplyGrowth` (`:169`) dereference `*m_pGrowthConfig` with no check, so the "supported" null case is a crash on the first growth turn. The `7` also re-hardcodes the population cap and duplicates `GrowthConfig_t`'s own default (`include/game/population/pop-types/GrowthConfigParser.h:11`), partially undoing prior finding 3.3, which recorded that the cap now comes from `pop_growth.json`. The same pattern repeats: `PopContainer.cpp:20` silently constructs a base with **zero pops** when the registry is null (then throws on the first `AddPop`), and `RecalculateComposition` (`PopulationManager.cpp:205-208`) returns silently with no calculator, so drone/talent rules simply do not run — which is exactly the state the test fixtures build (`tests/GameFixtures.h:113-124` passes a null composition calculator). Constructors should take these as references; the classes then cannot be built invalid and the silent no-ops disappear.

### [M] Golden-age inputs use `GetWorkerCount()`, which also counts drones and talents
`src/game/faction/base/population/PopContainer.cpp:35-38` — the predicate is `IsWorker() && !IsSpecialist()`, but `Pop::IsSpecialist()` is `!bCanWorkTile && riot == 0` (`src/game/population/pop-types/Pop.cpp:42-45`), so the second clause is always true here and the method returns *every tile-capable pop* — plain workers plus drones plus talents (asserted as such in `tests/faction/PopCompositionTests.cpp:70-71`). `CheckGoldenAgeEndOfTurn` feeds that number as `workerCount` alongside `talentCount` (`PopulationManager.cpp:243-248`) into a condition documented as `talents >= workers + specialists` (`include/game/population/calculators/GoldenAgeCalculator.h:9-10`). Because talents are counted on both sides, the effective rule becomes "every pop must be a talent" — far stricter than the stated one. The counts do not partition the population, and nothing says so. Not currently observable (`CheckGoldenAgeEndOfTurn` has no caller), but it is a wired-up wrong value, not a stub. Fix: expose a plain-worker count (`Pop::IsPlainWorker` already exists) and rename or drop the misleading `GetWorkerCount`.

### [M] The production queue has no contract for switching, surplus, or invalid input
`src/game/faction/base/production/ProductionManager.cpp:15-25,56-85` — three separate decisions are made implicitly, none documented in the header: switching items keeps the full mineral stockpile, so retargeting a 200-mineral project onto a 10-mineral facility completes it the next turn; `CompleteProduction` zeroes the stockpile (`:81`), silently destroying every surplus mineral above the cost; and `ApplyProduction` returns early when nothing is queued (`:58-61`), discarding its `minerals` argument — harmless only because `BaseProduction` skips empty bases first (`src/game/stages/BaseProduction.cpp:28-31`), so the second caller to exist loses minerals. Whatever the intended SMAC rules are, they should be stated (and, for a switch penalty, configurable) rather than emerging from where the assignments happen to sit.

### [M] `SetProduction` accepts any pointer and no layer validates the item
`src/game/faction/base/production/ProductionManager.cpp:15-25` — the item arrives as a bare non-owning `const IConstructable*` with `nullptr` overloaded as "clear"; there is no lifetime statement in the header and, more importantly, no validity check anywhere. `ProductionManager` has no access to `BuildingManager::GetBuildingsAvailableForConstruction`, so nothing rejects a building the base already owns, one whose tech is undiscovered, or a completed secret project — and the capture path re-installs the previous owner's item verbatim (`src/game/Faction.cpp:300-309`). On completion the id goes straight to `BuildingManager::AddBuilding`, which appends unconditionally. `CompleteProduction` also returns an empty string instead of throwing when nothing is queued (`:75-78`), a sentinel the project's guidelines rule out. Direction: take the item by reference with an explicit `ClearProduction()`, and give the manager a validity predicate (supplied by `BaseManager`) so an invalid item fails at selection time, loudly.

### [M] The `precedence` config key is parsed and ignored; the hardcoded order contradicts it
`src/game/faction/base/population/PopContainer.cpp:144-165` — plain workers are converted to drones first and to talents second. `config/pop_composition.lua` ships `precedence = { "Talent", "Drone", "Worker" }`, parsed into `PopCompositionConfig_t::precedence` and commented "Order in which types are assigned when recalculating" (`include/game/population/pop-types/PopCompositionConfigParser.h:17`), but no code reads it. When plain workers are scarce the order decides who gets promoted, so a modder editing `precedence` sees no effect and gets the opposite of what the file says. Either honour the field here or delete it from the config and parser — a config key that does nothing is worse than no key.

### [M] `~BatchCompositionUpdate` runs work that can throw
`src/game/faction/base/population/PopulationManager.cpp:110-117` — the destructor calls `RecalculateComposition`, which reaches `GetDefaultPopType_` (throws on a null registry, `:43-50`) and `PopContainer::ConvertTo` (throws on an unknown type id, `PopContainer.cpp:81-85`). Destructors are implicitly `noexcept`, so any of those throws terminates the process instead of propagating, and the batch is used on the hot worker-assignment paths (`WorkerAssignmentManager.cpp:122,209`). The drone/talent ids happen to be validated at load (`src/game/GameDataContext.cpp:130-141`), so this is latent rather than live today — but it is exactly the kind of trap that the null-registry case above can arm. Either make the deferred recalculation an explicit call at the end of the batching scope, or catch and report inside the destructor.

### [M] `RemovePop` is silent, arbitrary, and unobservable
`src/game/faction/base/population/PopContainer.cpp:66-72` — removal silently no-ops on an empty container while its sibling `AddPop` throws at max size (`PopulationManager.cpp:62-67`), and `PopulationManager::RemovePop` (`:73-77`) emits `OnPopLost` unconditionally, so a starving size-0 base reports pop losses that did not happen. Which pop dies is also policy hidden in the container: always `pop_back()`, so starvation can delete a user-assigned worker or a Doctor while plain workers remain. Finally, `OnPopLost(int newSize)` fires *after* destruction and does not identify the pop, so an observer holding a `Pop&` cannot invalidate it — the mirror-image of `UnitManager::OnUnitDestroyed(Unit&)`, which prior finding 1.5 introduced precisely to close that class of dangling reference. Emitting a `Pop&` before erasure would make the same guarantee available here.

### [M] Composition goes stale on every size change except growth
`src/game/faction/base/population/PopulationManager.cpp:79-102,221-229` — composition is recalculated automatically only when a conversion crosses the specialist boundary. `AddPop`/`RemovePop` do not mark it dirty, even though `targetDrones` is a function of `base_size` (`config/pop_composition.lua:2`). Growth is covered only because the `Population` stage recalculates after `ApplyGrowth` (`src/game/stages/Population.cpp:26-33`); the mid-turn removal paths (`BaseConquestEffects.cpp:45-53`, `ProbeActionEffects.cpp:171`) are not, so a base can spend the rest of the turn with a drone count its own formula disagrees with — and `IsRioting`, read by conquest and morale rules, is evaluated against those stale counts. Marking composition dirty in `AddPop`/`RemovePop` would make the invariant "counts always match targets" hold everywhere instead of stage-by-stage.

### [M] Composition's `psych_output` is specialist psych only
`src/game/faction/base/population/PopulationManager.cpp:210-214` and `PopContainer.cpp:168-176` — `psychOutput` is the sum of pop `GetSpecialistOutput().psych`, so the talent formula sees nothing from psych facilities or the energy allocation that `ResourceManager::GetPsychProduction` computes. The formula variable is named `psych_output`, which a modder will read as "this base's psych", and there is no TODO recording the narrower meaning. Either feed the base's real psych production or rename the input and say so in `PopCompositionCalculator`'s documented variable list.

### [M] Production's minerals-per-row is the one game number still in code
`include/game/faction/base/production/ProductionCostCalculator.h:16` — `k_MineralsPerRow = 10` is a compile-time constant, while the exactly parallel growth number lives in `config/pop_growth.json` as `nutrients_per_pop`. Prior finding 3.2 deliberately chose a static formula over Lua, which is reasonable, but it left the *number* unreachable: a mod can retune growth pacing and not production pacing. Moving it into a small production config struct (mirroring `GrowthConfig_t`) keeps the static formula and restores parity. Related: the `std::max(1, …)` floor (`ProductionCostCalculator.cpp:18`) means a config `base_cost` of 0 or a negative value silently becomes 1 mineral rather than being rejected at parse time — sane at runtime, but it hides bad config instead of failing loudly.

### [L] Convention and hygiene items
- `src/game/faction/base/population/PopContainer.cpp:37` — `!p->IsSpecialist()` is dead: `IsSpecialist()` cannot be true when `IsWorker()` is.
- `include/game/faction/base/population/PopContainer.h:74` — `CountPops_` takes a raw function pointer over `const Pop*`; a `const Pop&` predicate (template or `std::function`) matches the project's references-over-pointers rule and removes the `.get()` at the call site.
- `src/game/faction/base/population/PopContainer.cpp:135,147,159` — single-line `if (…) break;` next to fully braced blocks in the sibling loop at `:120-123`; brace style is mandated.
- `include/game/faction/base/population/PopulationManager.h:4` — includes `GrowthCalculator.h` but uses nothing from it in the header; the `.cpp` includes it independently.
- `src/game/faction/base/population/PopulationManager.cpp:34-36` — empty user-declared destructor; `src/game/faction/base/production/ProductionManager.cpp:7-13` — a constructor that restates the in-class initializers plus a defaulted destructor. Both suppress implicit moves for nothing.
- `include/game/faction/base/population/PopulationManager.h:95,101` — `CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` still have no callers anywhere (prior finding 5, recorded and still open); `IsRioting` therefore only ever becomes true through `ForceRiot`.

**Observed outside slice:**
- `src/game/faction/base/BaseManager.cpp:97-100` — `OnPopGained` re-runs `AutoAssignWorkers`, `OnPopLost` does not, so pops freed by a death or a composition-driven specialist demotion stay idle until the next event.
- `src/game/faction/base/buildings/BuildingManager.cpp:24-31` — `AddBuilding` appends without a duplicate check, so an unvalidated production item can install the same building twice.
- `src/game/Faction.cpp:448` — prints "workers:" using the over-counting `GetWorkerCount()`, so the log always includes drones and talents.
- `tests/GameFixtures.h:97-102` — fixtures never build `popCompositionCalculator` or `popTypeAvailabilityCalculator`, so every base-level test runs with composition rules disabled and `ConvertToFallback` unusable.

---

## Base management — resources and worker assignment

**Files:** `src/game/faction/base/resources/ResourceManager.cpp`, `include/game/faction/base/resources/ResourceManager.h`, `src/game/faction/base/resources/WorkerAssignmentManager.cpp`, `include/game/faction/base/resources/WorkerAssignmentManager.h`

**Assessment:** Prior finding 2.1 ("worker state in three classes") is genuinely fixed, and the
replacement is good: the authoritative record of "which tiles are worked" is `WorkedTileIndex`
(`include/game/map/WorkedTileIndex.h:65`, owned by `WorldMap`), an assignment *is* the RAII
`WorkedTileClaim` held by the `Pop` (`include/game/population/pop-types/Pop.h:81`), and "which
tiles *this base* works" is derived by iterating `m_rPops.Pops()` — there is no cached per-base
list to desync, and every mutation path (`Pop::Convert`, pop death, base founding, unit crawl
claims) releases through the same claim destructor, so the index cannot drift. What remains weak
is the *query* surface built on top of it: `IsTileAssigned` answers a world-wide question to
callers asking a per-base one, and `ResourceManager` is a thin but sloppy layer — six nullable raw
pointers, five near-identical stockpile accessors, and a base-level modifier pass that silently
drops every percentage effect.

### [H] `IsTileAssigned` answers "worked by anyone" to callers asking "worked by me"
`src/game/faction/base/resources/WorkerAssignmentManager.cpp:142-148` forwards to the world-scoped
index, which is correct for `GetAvailableTiles_` but wrong for the two UI callers, and the class
offers no per-base alternative. `BaseWorkableAreaDisplay::RenderTile_` uses it to choose between
`GetWorkedTileYield` and `GetPreviewTileYield` (`src/ui/base/BaseWorkableAreaDisplay.cpp:71,87-89`);
for a tile inside this base's radius that is worked by a neighbouring base, by another faction, by
one of this faction's supply crawlers, or that is another base's centre tile, the branch says
"worked" and `GetWorkedTileYield` then finds no matching pop and returns zeros
(`WorkerAssignmentManager.cpp:194`) — the player sees `0 0 0` in worked-tile colour for a tile that
in fact yields. Overlapping radii are the normal case, so this is reachable in any two-base start.
`BaseView::HandleTileClick_` has the mirror problem (`src/ui/base/BaseView.cpp:189-196`): it routes
such a click to `UserUnassignTile`, which scans only this base's pops and silently does nothing.
Fix: add an explicit "worked by this base" predicate (a pop scan, or `GetWorkedTileYield` returning
`std::optional`) and keep `IsTileAssigned` for the availability check it was written for.

### [M] `ResourceManager` takes six nullable pointers and re-checks them at every use
`include/game/faction/base/resources/ResourceManager.h:23-29,53-58` — every collaborator is a raw
`const T*`, so the constructor cannot produce a valid object and the class defends itself four
separate times (`.cpp:95-98`, `141`, `151`, `159`) with the same throw, while `m_pBaseTile` and
`m_pHomeUnits` are instead treated as *optional* (`.cpp:100,109`): a base constructed without its
centre tile silently produces less food forever rather than failing. `m_pBuildings` is stored and
never read at all. All six are non-null in the only construction site
(`src/game/faction/base/BaseManager.cpp:83-85`). Fix: take references for what is required, drop
`pBuildings`, and delete the scattered checks — guidelines call for constructors that produce valid
objects and for throwing rather than silently degrading.

### [M] `UserAssignBestAvailableWorker` ignores every failure and can strand a worker
`src/game/faction/base/resources/WorkerAssignmentManager.cpp:224-252` — all three strategies discard
`UserAssignWorker`'s `bool`. The last one is destructive: it unassigns the lowest-yield worker and
then tries to claim `pTile`; if the claim fails (tile not in the workable set, or claimed between
the caller's check and here) the worker has lost its tile and gained nothing, and the caller is
told nothing. The specialist branch is similar — it converts a specialist to a worker and leaves it
idle if the assignment fails. `BaseManager::UserAssignBestAvailableWorker` is a `void` pass-through
(`src/game/faction/base/BaseManager.cpp:189-192`), so the UI cannot report the failure either. Fix:
check the result and restore the previous assignment (or return a bool up to the UI).

### [M] The displaced-worker handler outlives the object it captures
`Assign_` hands `TryClaim` a `[this]{ AutoAssignWorkers(); }` callback
(`WorkerAssignmentManager.cpp:93-94`) that is stored inside the claim, i.e. inside the `Pop`.
`WorkedTileIndex.h:13-16` states the handler "must remain valid for the claim's whole lifetime", but
`BaseManager` constructs `m_pPopulation` before `m_pWorkerAssignments`
(`src/game/faction/base/BaseManager.cpp:75-80`), so the manager is destroyed *first* and every
surviving pop then holds a claim whose handler points at freed memory. Nothing invokes it in that
window today (only `ClaimDisplacing` fires handlers, and pop destruction does not), so this is a
latent hazard rather than a live crash — but the ordering cannot be fixed by reordering members
(the manager needs the population to construct). Fix: give `WorkerAssignmentManager` a destructor
(currently `= default`, `.h:30`) that clears every pop's claim.

### [M] Energy allocated to psych is a silent sink
`ConsumePsych()` (`ResourceManager.cpp:208-213`) has no caller anywhere outside tests — `IncomeCollection`
and `ResearchAccumulation` consume econ and labs (`src/game/Faction.cpp:81-101`), nothing consumes
psych. Because econ is the *residual* bucket (`src/game/faction/EconomyManager.cpp:48-56`), raising
the psych percentage provably removes energy from the treasury each turn and produces nothing: the
riot/golden-age path reads only specialist psych via `PopContainer::ComputePsychOutput`, never this
stockpile. This is the "stub wired so it silently produces wrong values" case rather than a missing
feature — the cost is already charged. Related: the stockpiles are accumulate-until-consumed with
no per-turn guard, so a mod that drops a consuming stage from `config/turn_stages.json` grows them
without bound silently.

### [L] Convention and hygiene items
- `WorkerAssignmentManager.h:50-57` — `ReleaseUserAssignment` / `ReleaseAllUserAssignments` have no
  callers anywhere (the latter not even in tests) and `ReleaseUserAssignment`'s body is identical to
  `UnassignWorker` (`.cpp:56-59` vs `103-106`); two public names for one operation, with docs that
  imply a distinction the claim model cannot express (the user flag lives on the claim, so it cannot
  be released without freeing the tile).
- `WorkerAssignmentManager.h:90-92` — `SetTileScorer` has no callers; it is the only reason `m_scorer`
  is a `std::function` (`.h:111`) rather than a private helper. Speculative generality.
- `WorkerAssignmentManager.cpp:319-333` — `PrioritizeAvailableTiles_` copies its argument and calls
  `m_scorer` (a full `ResolveTileYield`, including the neighbourhood scan) inside the sort comparator,
  i.e. O(n log n) resolves for n tiles; score each tile once into a pair vector and sort that.
- `WorkerAssignmentManager.cpp:348` — `availableTiles.erase(availableTiles.begin())` in a loop, and the
  tile is consumed even when `AssignWorker` returns false, silently skipping the pop.
- `WorkerAssignmentManager.cpp:16-19,298` — `IsUnassignedTile(pTile)` is a one-use alias for
  `pTile == nullptr`; `AutoAssignWorkers` then re-checks `pPop->IsWorker() && pPop->GetTile() == nullptr`
  (`.cpp:217`) on a vector `GetUnassignedWorkers_` just guaranteed those properties for.
- `WorkerAssignmentManager.cpp:39,44,72,142` — tiles are passed as nullable `const Tile*` everywhere and
  each function silently no-ops on null; guidelines prefer references, or a throw on an unexpected null.
- `ResourceManager.cpp:20-29` — `GetResourceValue_`'s `default: return 0` swallows a wrong `StatId_t`
  instead of throwing; the caller would then report modifiers as if they were production.
- `ResourceManager.cpp:38` — `if (!pUnit ...)` guards a container (`HomeBaseIndex::GetUnits`) that
  cannot hold nulls; either drop the check or throw.
- `ResourceManager.cpp:88-90,215-223` — empty out-of-line destructor, and `ProduceNutrients_` /
  `ProduceMinerals_` are single-line one-caller wrappers around `CalculateResource_`; the five
  `Consume*` bodies (`180-213`) are copy-paste of one another.
- `ResourceManager.cpp:165-177` — `GetEconProduction`/`GetLabsProduction`/`GetPsychProduction` each run a
  full `ComputeWorked_` pass plus a full `CalculateResource_(Energy, …)`; reading all five stats is five
  worked-tile passes. This is the memoization recorded as still-open follow-up under prior finding 1.1
  (`docs/code-review-findings.md:71-76`); `WorkedTileIndex::GetRevision()` is now available to key it on.

**Observed outside slice:**
- `src/game/faction/base/BaseManager.cpp:97-105` — `OnPopGained` triggers `AutoAssignWorkers`, but
  `OnPopLost` does not, so a tile freed by starvation stays unworked while fallback specialists exist.
- `src/ui/base/BaseView.cpp:189-196` — the tile-click toggle depends on the conflated predicate above;
  it will need updating with the fix.
---

## Map — runtime world model

**Files:** `src/game/map/TerraformSpread.cpp`, `include/game/map/TerraformSpread.h`,
`src/game/map/TerrainFeatureValidation.cpp`, `include/game/map/TerrainFeatureValidation.h`,
`src/game/map/TerritoryMap.cpp`, `include/game/map/TerritoryMap.h`, `src/game/map/Tile.cpp`,
`include/game/map/Tile.h`, `src/game/map/TileFlagMap.cpp`, `include/game/map/TileFlagMap.h`,
`src/game/map/TileLayerResolver.cpp`, `include/game/map/TileLayerResolver.h`,
`src/game/map/UnitPositionIndex.cpp`, `include/game/map/UnitPositionIndex.h`,
`src/game/map/WorkedTileIndex.cpp`, `include/game/map/WorkedTileIndex.h`,
`src/game/map/WorldMap.cpp`, `include/game/map/WorldMap.h`

**Assessment:** The occupancy indexes (`WorkedTileIndex`, `UnitPositionIndex`) are the
strongest part of this slice: RAII claims/registration, single writers for invariants, and
clear ownership under `WorldMap`. `Tile` as a plain data holder with mirrored terrain
feature pointers (plus `ValidateTerrainFeatures`) is coherent. The dominant weaknesses are
an ID-case bug in the layer resolver, a mutable escape hatch on the tile vector that can
dangle every `Tile*`-keyed index, and several silent no-ops where the project guidelines
require throws.

### [H] Query gameplay improvements with config ids, not sprite content ids
`src/game/map/TileLayerResolver.cpp:55-72` — `HasImprovement` is called with
`TileLayerContent::k_Farm` / `k_Forest` / `k_Road` (`"farm"` / `"forest"` / `"road"`), but
`config/improvements.json` ids are `"Farm"` / `"Forest"` / `"Road"`. Those checks always
fail, so Vegetation and Road layers never populate for real tiles. The skip list in
`ResolveImprovementLayer_` (`:86-88`) uses the same lowercase strings, so Farm/Forest/Road
fall through into the Improvement layer instead. Nothing calls `ResolveTileLayers` yet, but
the function is fully implemented and will mis-layer the first consumer; there are also no
tests. Direction: probe with the PascalCase improvement ids (or `magic_enum`/shared
constants) and keep returning the lowercase sprite content ids.

### [M] Stop exposing the owning tile vector as a mutable reference
`include/game/map/WorldMap.h:34-35` / `src/game/map/WorldMap.cpp:57-60` —
`GetTiles()` returns `std::vector<std::unique_ptr<Tile>>&`. Callers can `clear()`,
`reset()` elements, or reseat unique_ptrs while `UnitPositionIndex`, `WorkedTileIndex`,
bases, and units hold raw `Tile*` / `Tile&`. Address stability is load-bearing; this API
makes corruption a one-liner. Prior review §9 recorded this; it is still open. Direction:
expose a const span/range of `Tile&` (or const `unique_ptr` view) and keep mutation behind
world-gen/effects friends.

### [M] Make `TerritoryMap::Rebuild` fail loud on size mismatch
`src/game/map/TerritoryMap.cpp:116-121` — if unsized, `Rebuild` returns without updating
or throwing, so callers keep reading stale/`k_NoFactionOwner` as if ownership were current.
`ClaimFromBase_` (`:56-63`) indexes `visited` / `rBest` by base coordinates with no bounds
check against `m_width`/`m_height` and never asserts `rWorldMap` dimensions match — a
desynced `Reset` is undefined behavior on the origin write. Direction: throw unless
`IsSized()` and world width/height equal the grid; bounds-check the origin before indexing.

### [M] Do not swallow missing Forest/KelpFarm config during spread
`src/game/map/TerraformSpread.cpp:139-143` — `Find(improvementId)` returning null makes
`TrySpreadTerraformFromTile` return `false`, identical to “no eligible neighbor.” A modded
or incomplete `improvements.json` silently disables forest/kelp growth for the whole game.
`ValidateTerrainFeatures` does not cover these ids. Direction: use `Get` (throw) or validate
Forest/KelpFarm at load beside terrain enums.

### [M] Stop mutating fungus while probing spread eligibility
`src/game/map/TerraformSpread.cpp:89-100` — `PickBestSpreadNeighbor_` clears and restores
`SetHasFungus` on each candidate so `CanBuildImprovement` ignores the Fungus exclude. That
is a write in the middle of a selection pass (refreshes terrain-feature caches; any re-entrant
reader sees a lie), and it is not exception-safe if the check later grows throws. Direction:
test eligibility without touching tile state (e.g. treat Fungus as allowed for Forest spread
explicitly).

### [M] Reject non-positive `WorldMap` dimensions in the constructor
`src/game/map/WorldMap.cpp:8-21` — `width <= 0` or `height <= 0` yields an empty tile list,
`GetTile` always null (`:39-41`), and a zero-sized territory, with no throw. Guidelines
require constructors to produce valid objects. Direction: throw on non-positive dimensions.

### [L] Convention and hygiene items
- `src/game/map/TileLayerResolver.cpp:36-37` — `default:` on `Moisture_t` returns Arid and
  hides new enumerators from `-Werror=switch`.
- `include/game/map/Tile.h:3` — unused `#include <memory>`.
- `src/game/map/Tile.cpp:49-51` — empty user-declared destructor; prefer `= default` in the
  header if an out-of-line body is unnecessary.
- `include/game/map/Tile.h:79` — documents elevation range −4000…4000 but
  `SetElevation` (`Tile.cpp:95-98`) never enforces it.
- `src/game/map/TerraformSpread.cpp:79-107` — `bestScore` starts at `0` with `score > bestScore`;
  works for today’s ordinals but rejects a legitimate score of `0`; use a “found” flag or
  `INT_MIN`.
- `src/game/map/TileFlagMap.cpp:71-76` — out-of-bounds `Set` is a silent no-op (same pattern
  as `TerritoryMap::GetOwner`); prefer throw on unexpected coordinates once sized.
- `src/game/map/TileFlagMap.cpp:77` — stray trailing whitespace after the early-return brace.

**Observed outside slice:**
- `include/game/map/TileLayer.h:55-59` — lowercase `k_Farm`/`k_Forest`/`k_Road` are sprite
  content ids; the resolver bug above conflates them with improvement ids.
- `docs/architecture/map-system.md:117-120` — still describes
  `UnitPositionIndex::SetSingleUnitPerTile` / `TryMoveUnit`; stacking now lives in
  `MovementRules` and the API is `MoveUnit`.
- `docs/architecture/high-level.md` — still shows `TileMap` / `TileBonusRegistry` rather than
  the live `WorldMap` + indexes layout in `map-system.md`.

---

## Map — world generation and config

**Files:** `src/game/map/FbmNoise.cpp`, `include/game/map/FbmNoise.h`,
`src/game/map/FungusGeneration.cpp`, `include/game/map/FungusGeneration.h`,
`src/game/map/ImprovementConfigParser.cpp`, `include/game/map/ImprovementConfigParser.h`,
`src/game/map/LandmarkConfigParser.cpp`, `include/game/map/LandmarkConfigParser.h`,
`src/game/map/LandmarkGeneration.cpp`, `include/game/map/LandmarkGeneration.h`,
`src/game/map/MapGenerationConfig.cpp`, `include/game/map/MapGenerationConfig.h`,
`src/game/map/RiverGeneration.cpp`, `include/game/map/RiverGeneration.h`,
`src/game/map/TileBonusGeneration.cpp`, `include/game/map/TileBonusGeneration.h`,
`src/game/map/WorldGenDecorationConfigParser.cpp`, `include/game/map/WorldGenDecorationConfigParser.h`,
`src/game/map/WorldGenPresetConfigParser.cpp`, `include/game/map/WorldGenPresetConfigParser.h`,
`src/game/map/WorldGenerator.cpp`, `include/game/map/WorldGenerator.h`,
`include/game/map/MapUtils.h`, `include/game/map/WorldGenPresetRegistry.h`,
`include/game/map/WorldGenDecorationConfig.h`, `include/game/map/MoistureGeneration.h`,
`include/game/map/RockinessGeneration.h`, `include/game/map/TileLayer.h`,
`include/game/map/ImprovementRegistry.h`, `include/game/map/LandmarkConfig.h`,
`include/game/map/WorldGenPresetConfig.h`

**Assessment:** World gen is well factored for an in-progress system: elevation/FBM,
moisture/rockiness helpers, fungus/landmark/bonus placers, and JSON decoration/preset parsers
are separate and mostly throw on bad config. Cylinder wrap in `MapUtils` / noise sampling is
clear and consistent. The dominant weakness is pipeline ordering and exclusivity semantics —
landmarks reshape terrain and stamp `terminates_river` features after rivers are finalized,
and improvement coexistence is only half-enforced in the shared helper.

### [H] Reflow rivers after landmarks (elevation sculpt and terminators)
`src/game/map/WorldGenerator.cpp:51-57` — `GenerateAquifers_` calls `RecomputeRivers`
(`:280`) before `GenerateLandmarks_`. `ApplyMountPlanetSculpt_` then raises elevations and
changes rockiness (`LandmarkGeneration.cpp:97-128`), and `BoreholeCluster` stamps
`ThermalBorehole` (`terminates_river: true`) with no second reflow. Rivers keep flowing on
pre-landmark slopes and through boreholes that should stop them; orographic moisture is also
left stale on sculpted peaks. Direction: call `RecomputeRivers` (and reconsider moisture) after
landmark placement, or move sculpt/terminator landmarks before the aquifer/river stage.

### [H] Make `CanBuildImprovement` enforce both directions of `excludes`
`src/game/map/ImprovementConfigParser.cpp:96-104` — only `rCandidate.excludes` are checked
against the tile. Landmark configs exclude `@resource_bonus`, but `Nutrients`/`Minerals`/
`Energy` declare no excludes, so the shared helper would allow stacking bonuses on landmarks.
`TileBonusGeneration.cpp:18-63` duplicates a reverse scan (`TileExcludesBonus_`) to paper over
that; `LandmarkGeneration` and terraform callers do not. Direction: fold the reverse check into
`CanBuildImprovement` (or require symmetric excludes in data and delete the local helper) so
every placement path shares one rule.

### [M] Surface Mount Planet sculpt knobs in landmark/sculptor config
`src/game/map/LandmarkGeneration.cpp:97-128` — peak elevation caps (`3500`, `1000+t*2500`),
rocky core radius (`1.5f`), and the `"mount_planet"` id are hardcoded C++. Adding or tuning a
volcano landmark requires a code change despite `landmarks.json` already carrying radius.
Direction: put sculpt parameters on the shape/config (or a small sculptor table) and keep only
the algorithm in C++.

### [M] Reject invalid map dimensions instead of returning an empty world
`src/game/map/WorldGenerator.cpp:49-72` — `Generate` always constructs `WorldMap(width,
height)` with no check; `GenerateElevation_` returns early when `tileCount <= 0`, and later
stages no-op on an empty grid. Negative or zero sizes therefore produce a silent empty map
rather than a throw (against project preference). Direction: validate `width`/`height` (and
optionally seed) at the start of `Generate` and throw.

### [M] Do not silently skip empty landmark footprints
`src/game/map/LandmarkGeneration.cpp:236-240` — if `ExpandLandmarkShape` returns empty
(e.g. a mask with no `X`/`x` cells from `ExpandMask_` at `:66-94`), placement continues with
no error and that landmark is never attempted. Parser accepts such masks
(`LandmarkConfigParser.cpp:74-83`). Direction: throw at parse or place time when the expanded
footprint is empty.

### [M] Record the resolved seed when `seed == 0`
`src/game/map/WorldGenerator.cpp:44-47` — a zero seed picks `steady_clock` entropy into a
local and seeds `m_rng`, but never writes the effective seed back to
`MapGenerationConfig_t`. A “random” new game cannot be regenerated from settings. Direction:
return the resolved seed from `Generate` or write it through an out-parameter / mutable config
field the settings layer can persist.

### [L] Convention and hygiene items
- `src/game/map/WorldGenerator.cpp:302-306` — `RandomInt_` is unused dead code; remove or use it.
- `src/game/map/ImprovementConfigParser.cpp:100-101` — `CanBuildImprovement` uses a braceless
  `return false;` (brace style).
- `src/game/map/LandmarkConfigParser.cpp:17-22`, `MapGenerationConfig.cpp:15-18`,
  `WorldGenDecorationConfigParser.cpp:18-22` — identical `ToLower_` helpers copied three times.
- `src/game/map/WorldGenPresetConfigParser.cpp:58-65` — `ParseType_` is case-sensitive via
  `magic_enum::enum_cast`, while `ParseErosiveForces` lowercases; inconsistent wire tolerance.
- `src/game/map/LandmarkConfigParser.cpp:24-39` — domain parsing is a hand string switch;
  lowercase enum names match `magic_enum` after `ToLower_` (same pattern as erosive forces).
- `src/game/map/FungusGeneration.cpp:103-110` — frontier can enqueue the same tile repeatedly;
  correctness OK, but patch growth does needless work on large maps.

**Observed outside slice:**
- `docs/architecture/map-system.md:194-244` — still says world-gen does not place bonuses/fungus
  and lists fungus placement as future work; code in this slice already does both.
- `docs/architecture/high-level.md` — still diagrams `TileBonusRegistry` / separate tile-bonus
  config; bonuses are `ImprovementConfig_t` + `PlaceTileBonuses` now.

---

## Orbital systems

**Files:** `src/game/orbital/OrbitalAttack.cpp`, `include/game/orbital/OrbitalAttack.h`, `src/game/orbital/OrbitalCensus.cpp`, `include/game/orbital/OrbitalCensus.h`

**Assessment:** A small, readable rules surface: free functions, documented result DTOs, shared
`ReadyYearAfterDeploy`, and tests that pin hit/miss/deploy/census behavior. Error handling is
mostly consistent with project norms (throw on self-ASAT and unknown faction; `bAttempted == false`
for stale UI selections is intentional and documented). The dominant weakness is the destroy
path — a local helper that mirrors intercept destruction, omits secret-project tombstones, and
inherits faction deploy-bookkeeping ambiguities when multiple copies exist.

### [M] Tombstone secret projects destroyed by ASAT
`src/game/orbital/OrbitalAttack.cpp:31-42` — `DestroyOneBuilding_` calls `DestroyBuilding` and
`NotifyBuildingDestroyed` but never `GameState::MarkSecretProjectDestroyed`. Stock orbitals are
not secret projects, but a modded `orbital` + `secret_project` building destroyed on hit (or as
the attacker on fail) becomes buildable again, unlike base-raze tombstoning in
`BaseConquestEffects.cpp:106-111`. `TryAttackSatellite` already has `GameState&`; pass it into
the helper and mark when `pConfig->bIsSecretProject` (same gap exists in intercept — see outside).

### [M] Census count has two algorithms that can drift
`src/game/orbital/OrbitalCensus.cpp:18-32` vs `:49-66` — `BuildOrbitalCensus` tallies by walking
each base and counting `orbital == true` instances, while `CountFactionOrbitalBuildings` checks
one owned config’s `orbital` flag then returns `Faction::CountBuildings` (id match only). Today
every copy of an id shares one registry config, so they agree; a future per-instance flag or a
registry/ownership mismatch would desync the summary grid from `CountOrbitalBuildings`. Have the
single-id count reuse the same per-instance orbital check as `TallyOrbitalBuildings_` (or derive
census entries from repeated calls to one function).

### [L] Convention and hygiene items
- `src/game/orbital/OrbitalAttack.cpp:31-42` — `DestroyOneBuilding_` is a near-copy of
  `InterceptRules.cpp:178-188`; one `Faction` (or shared) helper would keep tombstone/deploy
  policy in a single place.
- `include/game/orbital/OrbitalAttack.h:3` and `include/game/orbital/OrbitalCensus.h:3` — both
  pull `BuildingConfigParser.h` (and thus `nlohmann/json`) for `BuildingConfig_t` /
  `BuildingId_t`; a leaner buildings types header would shrink UI/game include cost.
- `src/game/orbital/OrbitalAttack.cpp:93-95` vs `OrbitalCensus.cpp:56-57` — self-ASAT throws
  `logic_error`, unknown faction throws `runtime_error`; pick one for programmer/precondition
  errors.
- `src/game/orbital/OrbitalCensus.cpp:18-30` — `unordered_map` iteration makes
  `BuildOrbitalCensus` order nondeterministic; harmless for current UI (re-keys by faction+id)
  but brittle for golden tests that compare vectors.
- Test gap: no case for self-ASAT `logic_error` or `CountFactionOrbitalBuildings` on an unknown
  faction id (`tests/game/OrbitalCombatTests.cpp` covers the happy census/ASAT paths only).

**Observed outside slice:**
- `src/game/units/InterceptRules.cpp:178-188` — same missing secret-project tombstone on
  intercept fail-destroy.
- `src/game/Faction.cpp:197-208` — `NotifyBuildingDestroyed` comment claims it prefers a still-
  cooling deploy, but `find_if` takes the first matching id; expired deploys are never purged
  (`DeployBuilding` only appends), so a stale record can be erased instead of an active one when
  a copy is destroyed.
- `docs/architecture/high-level.md` — no orbital/census/ASAT subsystem; satellite UI and these
  free functions are invisible in the architecture diagrams.

---

## Population — calculators

**Files:** `src/game/population/calculators/GoldenAgeCalculator.cpp`, `include/game/population/calculators/GoldenAgeCalculator.h`, `src/game/population/calculators/GrowthCalculator.cpp`, `include/game/population/calculators/GrowthCalculator.h`, `src/game/population/calculators/PopCompositionCalculator.cpp`, `include/game/population/calculators/PopCompositionCalculator.h`, `src/game/population/calculators/PopTypeAvailabilityCalculator.cpp`, `include/game/population/calculators/PopTypeAvailabilityCalculator.h`, `src/game/population/calculators/RiotCalculator.cpp`, `include/game/population/calculators/RiotCalculator.h`

**Assessment:** `GrowthCalculator` and `GoldenAgeCalculator` are small, readable, and (for growth) already hardened by the prior 3.3 fix — `GrowthRate ≤ 0` blocks threshold growth explicitly, and the NearZeroGrowth/PopulationBoom gap is an honest TODO. The dominant weakness is `RiotCalculator`'s unfinished state machine (`ForceRiot` vs `Update`) and `PopTypeAvailabilityCalculator`'s obsolescence walk, which is order-dependent, non-transitive, and unbounded — fine for today's linear specialist chain, brittle for mods.

### [H] `ForceRiot` and `Update` disagree on what keeps a riot alive
`src/game/population/calculators/RiotCalculator.cpp:21-41` — `ForceRiot` sets `m_bRioting` and emits `OnIsRioting`, but `Update` clears that flag whenever `ComputeCondition_` is false. Probe Incite Drone Riots calls `ForceRiot` without changing composition (`src/game/units/ProbeActionEffects.cpp:137-139`), so a base that does not already have more drones than talents is only "rioting" until the next `Update`. `IsRioting` is already live for morale and probe rules (`MoraleCalculator.cpp`, `ProbeRules.cpp`); prior finding 5 noted `CheckRiotEndOfTurn` has no caller, but the calculator itself ships both halves of the machine. Wiring the existing EOT hook will silently undo every forced riot that natural counts would not sustain. Fix: sticky forced state (or a turn counter) that `Update` cannot clear until expiry, distinct from the drone>talent condition.

### [M] `ResolveCurrentType` can hang on an obsolescence cycle
`src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:65-94` — the `while (bChanged)` loop follows `obsoletes` edges with no visited set and no step limit. Stock data is a DAG (Doctor→Empath→Transcend), but a mod that makes A obsolete B and B obsolete A never exits. Throw after detecting a repeat of `resolvedId` (or after `GetAll().size()` steps).

### [M] Availability obsolescence is one level deep and order-tied
`src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:41-56` — `GetAvailable` only marks ids listed in `obsoletes` of types that are themselves currently available. `Transcend` obsoletes Empath/Thinker, not Doctor (`config/pop_types.json:159`); if Transcend's tech is present without Empath's, Doctor stays in the assignable list beside Transcend, and `ResolveCurrentType("Doctor", …)` (`:65-94`) also stays on Doctor because only Empath claims that edge. Stock play with a normal tech order hides this (Empath is available and strips Doctor), but any grant/mod that skips the middle tech desyncs UI availability from "current" specialist. Fix: close obsolescence under the available set (or walk the same chain `ResolveCurrentType` uses) and, when several successors obsolete one id, pick by an explicit rule rather than `GetAll()` order.

### [M] Composition calculator silently floors negative Lua results to zero
`src/game/population/calculators/PopCompositionCalculator.cpp:32-33` — after `EvalInt`, negative drone/talent targets become 0 with no error. That contradicts the project preference for throwing on bad values and stacks with `LuaRuntime::EvalInt` already returning 0 on formula errors (prior finding 3.6). A typo that yields −1 is indistinguishable from "no drones." Reject negatives (throw) or at least surface them; do not normalize.

### [M] No direct tests for three of five calculators
`RiotCalculator`, `GoldenAgeCalculator`, and `PopTypeAvailabilityCalculator` have no dedicated tests under `tests/` (only `GrowthCalculator` and composition-via-manager are covered). The ForceRiot sticky-state bug and the Doctor/Transcend obsolescence gap would both have been caught by a few focused cases on the calculator APIs alone.

### [L] Convention and hygiene items
- `include/game/population/calculators/PopCompositionCalculator.h:13-26` — `PopCompositionInputs` / `PopCompositionResult` are data structs without the required `_t` suffix; same for `RiotConditionInputs` (`RiotCalculator.h:11`).
- `include/game/population/calculators/PopCompositionCalculator.h:45-46`, `PopTypeAvailabilityCalculator.h:27` — collaborators stored as pointers after being taken as references in the constructor; `GoldenAgeCalculator` / `RiotCalculator` correctly keep `Signal<>&`.
- `include/game/population/calculators/RiotCalculator.h:15` — `targetTalents = -1` sentinel for "use talentCount"; an `std::optional<int>` (or a clearer inputs split) matches project style better than a magic negative.
- `src/game/population/calculators/GrowthCalculator.cpp:27` — `baseSize * nutrientsPerPop` multiplies as `int` before dividing by a `double`; cast either operand to `double` first so a large modded size cannot overflow before the scale is applied.
- `src/game/population/calculators/PopTypeAvailabilityCalculator.cpp:31,74` — repeated linear `std::find` over discovered techs; build an `unordered_set` once per call (same pattern both methods).

**Observed outside slice:**
- `src/game/faction/base/population/PopulationManager.cpp:243-248` — golden-age `workerCount` comes from `GetWorkerCount()`, which includes drones and talents (see review part 11); calculator formula is fine if plain workers are passed.
- `PopulationManager::CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` still have no callers (prior finding 5; incomplete relative to live `ForceRiot`).
- `docs/architecture/faction-system.md:266-267` — still describes `GrowthCalculator` as stateful Lua `threshold_formula`; implementation is static `nutrientsPerPop` / `GrowthRate`.

---

## Population — pop types and config

**Files:** `src/game/population/pop-types/GrowthConfigParser.cpp`, `include/game/population/pop-types/GrowthConfigParser.h`, `src/game/population/pop-types/Pop.cpp`, `include/game/population/pop-types/Pop.h`, `src/game/population/pop-types/PopCompositionConfigParser.cpp`, `include/game/population/pop-types/PopCompositionConfigParser.h`, `src/game/population/pop-types/PopTypeConfigParser.cpp`, `include/game/population/pop-types/PopTypeConfigParser.h`, `include/game/population/pop-types/PopTypeRegistry.h`

**Assessment:** This slice is small and mostly clear: `Pop` is a thin config-backed unit with tile claims and effect resolution split cleanly by scope (`ThisPop` vs `ThisBase`), and `PopTypeRegistry` enforces a single default at load. The dominant weaknesses are load-time permissiveness (growth and composition parsers accept incomplete/inert config) and role predicates that infer identity from contribution magnitudes, leaving an incomplete specialist check that will misclassify modded types.

### [M] Make `IsSpecialist` exclude talents (and any golden-age contributor)
`src/game/population/pop-types/Pop.cpp:42-45` — `IsSpecialist` is `!can_work_tile && riotContribution == 0`, which matches the header’s `!IsDrone()` wording but does **not** exclude `IsTalent()`. A non-worker with `golden_age_contribution > 0` is therefore both specialist and talent (`GetTalentCount` / `GetSpecialistCount` both count it; composition demotion of “excess talents” can convert specialists). Shipping types keep talents as workers, so this is latent — close the gap with `!IsWorker() && !IsDrone() && !IsTalent()` (or an explicit role flag in config). Related to the 3.4 role centralization, which stopped short of a closed role partition.

### [M] Inferring drone/talent identity from contribution thresholds couples role to magnitude
`src/game/population/pop-types/Pop.cpp:32-39` — `IsDrone` / `IsTalent` are `riotContribution > 0` / `goldenAgeContribution > 0`. That works for stock SMAC data, but any worker given a non-zero riot contribution becomes a composition “drone,” and any specialist given a golden-age contribution becomes a “talent” (see above). Composition already names conversion targets via `drone_type` / `talent_type`; role identity should be an explicit config field (or a closed enum) rather than a numeric threshold, so mods can grant partial contributions without changing type identity.

### [M] `GrowthConfigParser` silently defaults missing or mistyped keys
`src/game/population/pop-types/GrowthConfigParser.cpp:20-21` — Uses `json.value(..., config.*)` so omitted/`nutrients_per_po` typos load as `10` / `7` with no error, and there is no check that either value is positive (`nutrients_per_pop == 0` makes the growth threshold always 0). Contradicts the project rule against warn-and-default parsers (finding 3.8’s adopted style). Require the keys (e.g. `.at()`), validate `> 0`, and keep struct defaults only for programmatic construction.

### [M] Composition parser allows empty formulas and records unused `precedence`
`src/game/population/pop-types/PopCompositionConfigParser.cpp:27-28,43-52` — `drone_type` / `talent_type` are required, but formulas default to `""`; `LuaRuntime::EvalInt` then returns 0, so a missing formula silently means zero drones/talents forever. `precedence` is parsed into `PopCompositionConfig_t` and documented as controlling recalculation order (`PopCompositionConfigParser.h:17`) but is never read anywhere — modders who reorder it get no effect. Require non-empty formulas at parse time; either wire `precedence` or drop it until implemented (TODO elsewhere), and reject non-string precedence entries instead of skipping them.

### [M] `PopTypeRegistry::Validate_` does not check internal pop-type references
`include/game/population/pop-types/PopTypeRegistry.h:27-45` — Only enforces exactly one `is_default`. `fallback_pop_type` and `obsoletes` ids are never checked here (required tech is validated elsewhere). A typo in `fallback_pop_type` fails only at `ConvertToFallback` runtime; a bad `obsoletes` entry is silently inert in `PopTypeAvailabilityCalculator::ResolveCurrentType`. Extend `Validate_` to require every non-empty fallback/obsolete id exists in the registry (mirror `TechRegistry` prerequisite checks).

### [L] Convention and hygiene items
- `include/game/population/pop-types/PopTypeConfigParser.h:15-22` — `PopTypeConfig_t` bools/ints lack in-class initializers (unlike `GrowthConfig_t`); a default-constructed value is indeterminate until every field is assigned.
- `src/game/population/pop-types/Pop.cpp:111` vs `:130` — `ApplyTileMultipliers` rounds resolved totals; `GetSpecialistOutput` truncates via `static_cast<int>`. Align rounding once fractional specialist modifiers appear.
- `include/game/population/pop-types/GrowthConfigParser.h:17-18`, `PopCompositionConfigParser.h:23-24`, `PopTypeConfigParser.h:29-30` — Explicit empty default ctor/dtor noise; prefer `= default` only where needed or omit.
- `include/game/population/pop-types/Pop.h:41` — Comment says `!IsDrone()`; implementation uses `riotContribution == 0` (equivalent today) but neither mentions talents — keep comment in sync with the predicate fix above.

**Observed outside slice:**
- `docs/architecture/faction-system.md:266-267` — Still describes `GrowthConfigParser` loading `pop_growth.lua` / `threshold_formula`; live path is `pop_growth.json` + `nutrientsPerPop` / `maxBaseSize`.
- `config/pop_growth.lua` — Unused leftover alongside `config/pop_growth.json`.
- `src/game/GameDataContext.cpp:130-141` — Validates composition type ids exist, not that they satisfy `IsDrone` / `IsTalent`.
- `src/game/faction/base/population/PopContainer.cpp:144-151` — Local `currentDrones++` after `ConvertTo` does not re-check `IsDrone()`, so a mis-set `drone_type` can stop the loop without creating drones.

---

## Research — tech registry and costs

**Files:** `src/game/research/TechConfigParser.cpp`, `include/game/research/TechConfigParser.h`, `src/game/research/TechCostCalculator.cpp`, `include/game/research/TechCostCalculator.h`, `src/game/research/TechCostConfigParser.cpp`, `src/game/research/TechRegistry.cpp`, `include/game/research/TechRegistry.h`, `include/game/research/TechCostConfig.h`

**Assessment:** This is a small, readable slice: tech data is a plain `TechConfig_t`, the registry only adds prereq validation on top of `Registry`, and cost evaluation is a thin Lua bridge matching the pop-composition calculator pattern. The dominant weakness is silent acceptance of bad cost configuration — empty or failed formulas and missing `cost` fields degrade into a plausible minimum cost of 1 instead of failing at load.

### [H] Reject empty cost formulas; do not floor eval failures into cost 1
`src/game/research/TechCostConfigParser.cpp:24` — `cost_formula` defaults to `""` when absent. `src/game/research/TechCostCalculator.cpp:34-36` then evaluates that string and clamps with `std::max(1, cost)`. `LuaRuntime::EvalInt` returns `0` for an empty formula (and on formula errors — see prior finding 3.7 in `docs/code-review-findings.md`); the calculator turns that into research cost **1** with no throw. A missing or broken mod formula therefore looks like a valid cheap tech rather than a load-time failure. Require a non-empty `cost_formula` in the parser (as `PopCompositionConfigParser` requires `drone_type` / `talent_type`), and treat non-positive / failed evaluation as an error instead of silently clamping.

### [M] Prerequisite validation misses cycles
`src/game/research/TechRegistry.cpp:21-39` — self-reference and unknown prereq ids throw, but A→B→A (or longer cycles) load successfully. `ResearchManager::GetAvailableTechs` (caller) only unlocks techs whose prereqs are already discovered, so a cyclic component is permanently unreachable with no startup error. Add a cycle check in `Validate_()` (DFS / topo over the prereq graph) so bad mod trees fail at load.

### [M] Missing tech `cost` silently becomes 0
`src/game/research/TechConfigParser.cpp:26` — `techJson.value("cost", 0)` invents a zero base cost when the key is absent, against the project preference to throw on bad config. `base_cost` is already passed into the Lua vars (`TechCostCalculator.cpp:31`) and is documented as part of the formula surface (`config/tech_cost.lua`); once the formula uses it, a typo’d omission silently cheapens the tech. Require the `cost` field (allow explicit `0` if free techs are intentional).

### [M] Tech cost script load errors drop Lua diagnostics
`src/game/research/TechCostConfigParser.cpp:17-20` — on `safe_script_file` failure the throw is only `"Failed to load tech cost script '" + path + "'"`. The sibling `PopCompositionConfigParser` appends `sol::error::what()`. Mod authors get no line/message for a broken `tech_cost.lua`. Include the Lua error text in the exception.

### [L] Convention and hygiene items
- `include/game/research/TechCostCalculator.h:15` / `src/game/research/TechCostCalculator.cpp:14-16` — empty user-declared destructor; use `= default` in the header like `PopCompositionCalculator`.
- `src/game/research/TechCostConfigParser.cpp:3` — unused `#include <iostream>`.
- `include/game/research/TechRegistry.h:19` vs `src/game/research/TechRegistry.cpp:21` — header parameters are `rConfig` / `rConfigs`; definition uses `config` / `configs` / `c` (missing `r` reference prefix).
- `include/game/research/TechConfigParser.h:26-27` — empty constructor body in the `.cpp`; prefer `= default` in the header.
- `include/game/research/TechCostCalculator.h:23` — closing brace lacks `// namespace ac`.
- No dedicated tests for `TechCostCalculator` / `TechRegistry` validation (empty formula, missing cost, prereq cycles); only indirect use via research/effects fixtures.

**Observed outside slice:**
- `docs/architecture/research-system.md` — stale: documents `TechId` as `int`, a hardcoded `baseCost * (1 + 0.5 * missingPrereqs) * multiplier` formula, and registry “filter available techs” responsibility that now lives on `ResearchManager`.
- `src/lib/LuaRuntime.cpp:37-70` — `EvalInt` warns and returns `0` on formula errors (prior finding 3.7); that failure mode is what `TechCostCalculator`’s floor turns into cost 1.

---

## Social engineering — policies and ratings

**Files:** `src/game/social-engineering/SocialPolicyConfigParser.cpp`, `include/game/social-engineering/SocialPolicyConfigParser.h`, `src/game/social-engineering/SocialRatingConfigParser.cpp`, `include/game/social-engineering/SocialRatingConfigParser.h`, `src/game/social-engineering/SocialRatingResolver.cpp`, `include/game/social-engineering/SocialRatingResolver.h`, `include/game/social-engineering/SocialEffects.h`, `include/game/social-engineering/SocialRatingRegistry.h`, `include/game/social-engineering/SocialPolicyRegistry.h`, `include/game/social-engineering/SocialRatingConfig.h`, `include/game/social-engineering/SocialPolicyConfig.h`

**Assessment:** Policy config + `IsAvailable`, registries, and the SMAC clamp path (`ClampSocialRatingTotal` / `FindSocialRatingLevelEffects` / per-base `ExpandSocialRatingEffects`) are clear and match the two-level design; faction-lane expansion (previously accumulating over the raw faction pool) was fixed by effects package 2, which also collapsed both expands onto one shared append helper. What remains is rating load, which still lags behind the shared `JsonConfigLoader` / `ConfigFields` pattern the policy parser already uses.

### [M] Rating parser still hand-rolls file load and weak field access
`src/game/social-engineering/SocialRatingConfigParser.cpp:17-66` — `ParseConfig` duplicates `JsonConfigLoader::LoadFile` (open / array check / loop / cout), while `SocialPolicyConfigParser` already uses the shared helper. `ParseRatingConfig` uses `operator[]` for `id` / `levels` instead of `ConfigFields::ParseId` and `.at("levels")`, so a missing `levels` object can yield an empty table instead of a load error. Route through `JsonConfigLoader::LoadFile` + `ConfigFields` and require `levels` to be a JSON object.

### [M] Missing rating-table entry silently drops the axis
`src/game/social-engineering/SocialRatingResolver.cpp:31-35`, `39-43` (`AppendRatingLevelEffects_`) — a non-zero accumulated total whose axis is absent from `SocialRatingRegistry` (or has an empty `levelEffects`) is skipped with no error. Modifier parse already constrains `SocialRatingId_t`; a missing table row is a config/registry defect. Prefer `Get` / throw on unknown axis when `total != 0`, consistent with the project’s required-id rule.

### [M] Delete unused `SocialScores` stub type
`include/game/social-engineering/SocialEffects.h:6-18` — `SocialScores` is never included or referenced anywhere in the tree. It presents a parallel per-field score model next to the real `SocialRatingId_t` + map accumulation path and will mislead the next reader. Remove the header (or replace it only when a real DTO is needed).

### [L] Convention and hygiene items
- `include/game/social-engineering/SocialEffects.h:6` — `SocialScores` omits the `_t` data-struct suffix required by coding guidelines.
- `include/game/social-engineering/SocialPolicyRegistry.h:15` — parameter `rCategory` is passed by value; `r` prefix is for references.
- `include/game/social-engineering/SocialPolicyConfigParser.h:4` — `#include "game/effects/EffectConfigParser.h"` is unused in the header (only the `.cpp` needs it).
- `include/game/social-engineering/SocialPolicyConfigParser.h:15-16`, `SocialRatingConfigParser.h:14-15` — empty public default constructors add nothing; prefer `= default` on the declaration or omit.
- `src/game/social-engineering/SocialRatingResolver.cpp:80-85` — `ClampSocialRatingTotal` is UB on empty `levelEffects` (`begin()`/`rbegin()`); public API documents the precondition but does not enforce it (assert or throw).

**Observed outside slice:**
- `docs/architecture/high-level.md:365` — still says `EffectConfig_t` “will eventually” live on `SocialPolicyConfig_t`; policies already store `effects`.

---

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

---

## Turn stages — late pipeline and custom

**Files:** `src/game/stages/CustomTurnStage.cpp`, `include/game/stages/CustomTurnStage.h`,
`src/game/stages/Save.cpp`, `include/game/stages/Save.h`, `src/game/stages/TurnEnd.cpp`,
`include/game/stages/TurnEnd.h`, `src/game/stages/Upkeep.cpp`, `include/game/stages/Upkeep.h`,
`src/game/stages/VictoryConditionChecks.cpp`, `include/game/stages/VictoryConditionChecks.h`,
`src/game/stages/WorldEvents.cpp`, `include/game/stages/WorldEvents.h`

**Assessment:** `Save`, `TurnEnd`, `Upkeep`, and `VictoryConditionChecks` are honest
placeholder stages (log + `Continue`) and are fine as such. `CustomTurnStage` is a thin,
correctly typed hook-only fallback with a useful construction guard. The only stage in this
slice with real rules is `WorldEvents`, and its RNG and year-index choices are the dominant
weakness: forest/kelp growth is live gameplay that does not participate in the game's RNG
ownership model and hardcodes a year epoch already owned elsewhere.

### [H] WorldEvents forks a private RNG from year and map area alone
`src/game/stages/WorldEvents.cpp:24-30` builds a fresh `std::mt19937` seeded only from
`turnIndex` and `width * height`, then passes that into `SpreadTerraformImprovements`.
`GameState` already owns a shared combat/probe stream (`m_rng` at
`include/game/GameState.h:196`, seeded once in `GameState.cpp:48`), but WorldEvents never
touches it — and there is no accessor today, so the stage invents a second deterministic
universe. Consequence: every game with the same map dimensions gets the identical
forest/kelp sample sequence every year, independent of map contents or the game's random
device seed. Direction: drive spread from the game RNG (expose it on `GameState` if needed)
so world growth shares the same stream policy as combat and probes; do not re-seed from
year × area each turn.

### [M] First-playable-year epoch is hardcoded beside GameState's starting year
`src/game/stages/WorldEvents.cpp:23-24` subtracts literal `2100` to form SMAC's
`CurrentTurn`-style index. `GameState.cpp:30-32` already documents and owns
`k_StartingMissionYear = 2099` so the first `TurnStart` increment lands on 2100. Those two
constants are not linked: changing the starting year (or making it config) silently skews
spread attempt rates via `TerraformSpreadGrowthAttempts` without a compile error. Direction:
share one named epoch (constant or `GameState` helper such as years-since-start) and use it
here.

### [M] Custom stage "at least one hook" guard does not enforce its own invariant
`include/game/stages/CustomTurnStage.h:14` and `CustomTurnStage.cpp:10-15` exist so a
hook-only stage cannot "silently do nothing", but `RequireAtLeastOneHook` only checks that
the pre/post/replace *lists* are non-empty. `HookContext` treats presence as
`!vector.empty()` (`HookContext.cpp:62-74`) and only invokes `callback` when it is set
(`:55-58`). Nothing in the repo ever assigns `Hook_t::callback`, so the shipped
`CustomModStage` entry (`config/turn_stages.json:135-148`) passes construction, skips
`ExecuteImpl` via replace-hook presence, and every turn becomes a cout-only no-op. This is
the Custom*-side incomplete edge of prior 1.10 / the turn-pipeline replace-hook finding:
tighten the guard to require a callable callback (or fail config load when `script_path`
cannot be bound), so an empty mod stage fails at startup instead of sitting in the default
turn order.

### [L] Convention and hygiene items
- `include/game/stages/CustomTurnStage.h:26,39` — `m_name` is stored after construction only
  for the ctor error path; once `RequireAtLeastOneHook` returns it is unread.
- `include/game/stages/CustomTurnStage.h:19,32` — `name` is a `const std::string&` parameter
  without the project `r` reference prefix (`rName`).
- `include/game/stages/CustomTurnStage.h:23,36` — `ExecuteImpl` bodies live inline in the
  header while every sibling stage in this slice keeps `ExecuteImpl` in the `.cpp`.
- `src/game/stages/WorldEvents.cpp:25-30` — binds a `const WorldMap&` solely for the seed,
  then calls non-const `GetWorldMap()` again for the spread; one mutable reference is enough.
- `src/game/stages/WorldEvents.cpp:28` — `GetWidth() * GetHeight()` is signed `int`
  multiplication before the `unsigned` cast; large maps hit UB before seeding.
- `src/game/stages/Save.cpp`, `TurnEnd.cpp`, `Upkeep.cpp`, `VictoryConditionChecks.cpp` —
  four identical stub shapes (ctor + cout + `Continue`); fine while empty, but any shared
  logging/TODO pattern should be one helper before a fifth copy appears.
- No stage-level test exercises `WorldEvents::ExecuteImpl` (only
  `SpreadTerraformImprovements` in `tests/effects/TileEffectsTests.cpp`); the year→index and
  RNG wiring above are therefore unguarded.

**Observed outside slice:**
- `docs/architecture/turn-system.md:205-207` still lists `WorldEvents` among placeholder
  logging stages; it now mutates the map via terraform spread and the diagram/docs should
  say so.
- `docs/architecture/turn-system.md:220` still says `Engine` increments the mission year
  after `ProcessTurn`; `TurnStart` does that now (`TurnStart.cpp:24`).
- `src/game/council/CouncilOutcomeApplier.cpp:39` still expects a WorldEvents trigger API
  for sea-level / world-parameter outcomes; none exists on this stage yet (unimplemented,
  but the coupling is already documented).
- `GameState::m_rng` has no public accessor (`include/game/GameState.h:196`); fixing the
  WorldEvents RNG finding needs a small API addition there or an equivalent injection point.

---

## Units — model, orders, movement

**Files:** `FoundBaseRules.{h,cpp}`, `MoveCostCalculator.{h,cpp}`, `MovementRules.{h,cpp}`, `Pathfinder.{h,cpp}`, `StepEvaluator.{h,cpp}`, `TerraformRules.{h,cpp}`, `TransportRules.{h,cpp}`, `Unit.{h,cpp}`, `UnitComponentConfig.h`, `UnitComponentConfigParser.{h,cpp}`, `UnitComponentRegistry.h`, `UnitDesign.{h,cpp}`, `UnitOrder.{h,cpp}`, `UnitOrderExecutor.{h,cpp}`, `UnitSlotConfig.h`, `UnitSlotConfigParser.{h,cpp}`, `UnitSlotRegistry.h`, `UnitDomain.h`, `IUnitOrderWorld.h`, `MovementConstants.h`

**Assessment:** Movement is well factored along the architecture doc: `MoveCostCalculator` owns entry terms, `StepEvaluator` owns legality (objective vs faction-known), `Pathfinder` plans, `UnitOrderExecutor` mutates. Transport boarding vs unaided occupation is clear and tested. The dominant weaknesses are execution-path cost (full-map hostile scans and full Dijkstra per step) and a few silent failure / two-phase-init traps that will bite as conquest and terraform see more use.

### [H] Stop scanning the whole map for hostiles on every step
`src/game/units/UnitOrderExecutor.cpp:73-94` — `CollectVisibleHostileIds_` walks every tile and every unit, calling `IsUnitVisibleTo` each time. `TryStep` (`:218-219`, `:99-100`) does this twice per attempted step (before + after). A multi-step `Execute_` move therefore pays O(steps × tiles × units × visibility work) even in empty fog. On a real map this will dominate the PlayerActions pass. Restrict the scan to tiles whose visibility changed this step (or maintain a faction-visible-hostile set invalidated by reveal / move).

### [H] Stacking left the position index; the process-global is an incomplete prior fix
`src/game/units/MovementRules.cpp:21-42,130-143` — prior review 2.2 recorded stacking as enforced inside `UnitPositionIndex::TryMoveUnit` / `SetSingleUnitPerTile`. That API is gone: `MoveUnit` no longer checks capacity, and the only switch is file-scope `s_bSingleUnitPerTile` consulted by `CanPlaceUnitOnTile` / `StepEvaluator`. Any caller that moves without the step check can overstack, and tests can leak the global across cases. Put the rule on world/game config owned beside the index, and enforce it at the same mutation boundary that updates occupancy (or make `MoveUnit` reject illegal stacks).

### [M] Terraform completion ignores apply failure after energy was spent
`src/game/units/UnitOrderExecutor.cpp:504-518` — after counting down `TerraformOrder_t`, `ApplyTerraformResult`’s `bool` is discarded and the order always `Complete`s. Energy was already debited in `TryStartTerraform` (`:363-366`). If another former changes the tile mid-project, or the improvement id disappears from the registry (`:506-509` also completes with no apply), the player loses turns and energy with no mutation and no refund. Surface failure (keep/retry/refund) instead of treating a failed apply as success.

### [M] Conquest depends on a post-ctor nullable `GameDataContext`
`include/game/units/UnitOrderExecutor.h:57-59` / `src/game/units/UnitOrderExecutor.cpp:155-160,296-303` — `m_pGameData` is set only via `SetGameDataContext`. With `m_pWorld` bound but data unset, `ApplyArrivalEffects_` and post-combat conquest silently no-op (no capture, no raid). Same two-phase pattern as `DiplomaticActionExecutor` (prior 4.2 / slice 09). Take `const GameDataContext&` in the constructor when a world is supplied, or fail loudly if conquest is invoked unbound.

### [M] `NextStep` always runs a full Dijkstra
`src/game/units/Pathfinder.cpp:144-151` / `UnitOrderExecutor.cpp:414-415` — re-planning after each step is required (fog/contact), but `NextStep` materializes the entire path (`FindPath`) only to return `tiles.front()`. Every move fragment spent pays a full O(tiles log tiles) search plus two `tileCount`-sized vectors. Early-exit Dijkstra (stop when the first step off the origin is finalized) or a search that returns only the successor would keep the semantics at a fraction of the cost.

### [M] Sea-former domain rules hardcode improvement ids
`src/game/units/TerraformRules.cpp:47-49` — `KelpFarm` / `MiningPlatform` / `TidalHarness` are special-cased by id, while `config/improvements.json` already tags them `sea_terraform`. A modded sea improvement with the tag but a new id is treated as land-only (or wrongly allowed). Drive `DomainAllows_` from tags / config fields, not a closed id list.

### [M] `EmbarkInto` does not enforce carrier invariants
`src/game/units/Unit.cpp:128-140` — public `EmbarkInto` links cargo with no same-tile, capacity, domain, or faction checks (`TransportRules` documents those as caller duties). A single missed call site overfills `m_cargo` or embarks across tiles; `FreeCargoSlots` then goes negative and `MoveUnit` will still tow the passenger. Enforce the predicates inside `EmbarkInto` (or make it private to `TransportRules`).

### [L] Convention and hygiene items
- `src/game/units/MoveCostCalculator.cpp:25` — `k_RoadId = "Road"` couples fungus-as-road to a magic improvement id; prefer a config/tag look-up.
- `src/game/units/StepEvaluator.cpp:26` — embarked-in-base test uses `HasImprovement("Base")` string rather than the founding/base-tile predicate used elsewhere.
- `src/game/units/UnitOrderExecutor.cpp:393-394`, `MoveCostCalculator.cpp:118-119` — single-line `if` bodies omit braces required by project style.
- `src/game/units/TerraformRules.cpp:80-91,216-244` — raise/lower elevation bands (`1000` / `3500`) are magic numbers with no named constants or config.
- `src/game/units/UnitComponentConfigParser.cpp:14-23` — combat-rating target map is fine (wire form ≠ enumerator), but sits far from `CombatRatingTarget_t`; keep the map next to the enum per guidelines.
- `include/game/units/UnitOrderExecutor.h:25` — forward-declares unused `GameState` (only needed in the `.cpp` / method signatures that already include it).

**Observed outside slice:**
- `src/game/units/AttackRules.cpp:78` — declare-attack allows any positive fragment balance while `TryAttack` always deducts a full move point (clamped), so a 1-fragment unit can still attack.
- `src/game/map/UnitPositionIndex.cpp:17-36` — `MoveUnit` trusts callers for stacking; pairs with the incomplete 2.2 fix above.
- `src/game/faction/UnitVisibility.cpp` — `IsUnitVisibleTo` re-collects tile area effects per channel; amplifies `CollectVisibleHostileIds_` (already noted in slice 09).

---

## Units — combat, probes, conquest, morale

**Files:** `src/game/units/AttackRules.cpp`, `include/game/units/AttackRules.h`, `src/game/units/BaseConquestConfigParser.cpp`, `include/game/units/BaseConquestConfigParser.h`, `src/game/units/BaseConquestEffects.cpp`, `include/game/units/BaseConquestEffects.h`, `src/game/units/BaseConquestRules.cpp`, `include/game/units/BaseConquestRules.h`, `src/game/units/CombatResolver.cpp`, `include/game/units/CombatResolver.h`, `src/game/units/DisengageRules.cpp`, `include/game/units/DisengageRules.h`, `src/game/units/InterceptRules.cpp`, `include/game/units/InterceptRules.h`, `src/game/units/MoraleCalculator.cpp`, `include/game/units/MoraleCalculator.h`, `src/game/units/MoraleConfig.cpp`, `include/game/units/MoraleConfig.h`, `src/game/units/MoraleConfigParser.cpp`, `include/game/units/MoraleConfigParser.h`, `src/game/units/ProbeActionConfigParser.cpp`, `include/game/units/ProbeActionConfigParser.h`, `src/game/units/ProbeActionEffects.cpp`, `include/game/units/ProbeActionEffects.h`, `src/game/units/ProbeActionExecutor.cpp`, `include/game/units/ProbeActionExecutor.h`, `src/game/units/ProbeRules.cpp`, `include/game/units/ProbeRules.h`, `src/game/units/ProbeTarget.cpp`, `include/game/units/ProbeTarget.h`, `include/game/units/BaseConquestConfig.h`, `include/game/units/ProbeActionResult.h`, `include/game/units/ProbeActionConfig.h`

**Assessment:** The slice is thoughtfully layered — pure predicates (`AttackRules`, `BaseConquestRules`, `DisengageRules`, `ProbeRules`) stay separate from world mutation (`CombatResolver`, `BaseConquestEffects`, `ProbeActionEffects`) and orchestration (`ProbeActionExecutor`). Headers document invariants clearly, and config drives conquest/morale/probe tunables. The dominant weakness is incomplete wiring of already-declared combat/probe mechanics (disengage chance, intercept source identity, sabotage/deploy bookkeeping), so stock config values and effect conditions can silently disagree with runtime behavior.

### [H] Roll `DisengageChance` before committing a withdrawal
`src/game/units/CombatResolver.cpp:45-75` — `TryDisengage_` moves the unit as soon as eligibility, half-HP, and a retreat tile exist; it never reads `StatId_t::DisengageChance`. Stock Speeder chassis grants `disengage_chance: 25` (`config/unit_components/chassis.json:41`), and `docs/game-rules/unit-components.md` defines that stat as a percent chance, so eligible units always withdraw. Roll the resolved chance (e.g. via `RollPercent`) after the half-HP gate and before `MoveUnit`.

### [H] Keep the originating base on building intercept candidates
`src/game/units/InterceptRules.cpp:100-107` / `166-189` — ThisBase intercepts are collected from a specific `BaseManager`, but `InterceptCandidate_t` only stores `sourceId`. On fail-destruction, `MaybeDestroyInterceptSourceOnFail_` calls `Faction::FindBaseWithBuilding`, which returns the first base owning that building id. With the same building in two bases (or FactionGlobal charges), the wrong copy is destroyed and `NotifyBuildingDestroyed` clears a deploy against the wrong inventory. Store a `BaseManager*` (or base id) on the candidate when `sourceKind == Building` and destroy through that base.

### [M] Probe sabotage skips deploy-ledger notification and conflates facility vs random
`src/game/units/ProbeActionEffects.cpp:105-134` — `DestroyBuilding` is called without `Faction::NotifyBuildingDestroyed`, unlike `BaseConquestEffects` / intercept fail paths; a sabotaged ODP (or any cooling deploy source) leaves `m_buildingDeploys` stale so `CountReadyBuildings` under-counts. Separately, `SabotageFacility` with an empty `facilityId` falls through to the random/production-wipe branch, and a non-empty missing id still reports `ProbeDestroyedFacility_t` after `DestroyBuilding`'s documented no-op. Require a present building for targeted sabotage, notify the faction on every real destroy, and do not claim success when nothing was removed.

### [M] Intercept condition context marks the wrong combat role
`src/game/units/InterceptRules.cpp:93` — Candidates are filtered with `EffectContext_t{&rDefender.GetTile(), CombatRole_t::Attacker}` and never set `pAttacker`. `IsDefending` therefore always fails for intercept effects, and `AttackerIsEmbarked` is always false even though `UnitFilterSatisfied` already has the live attacker. Use `CombatRole_t::Defender` (or document a dedicated intercept role) and set `pAttacker = &rAttacker`.

### [M] Unit-subvert cost treats HQ-tile distance 0 as the no-HQ default
`src/game/units/ProbeRules.cpp:107-112` — `QuoteSubvertUnitCost_` replaces `distToHq <= 0` with `k_defaultHqDistance` (12), while the base mind-control quote returns `nullopt` for the same case (`88-95`). A unit on the HQ tile therefore uses denominator `12 + distBias` instead of being refused or priced at true distance 0 (`distBias` only) — with stock `dist_bias: 2` that makes HQ-garrison subversion much cheaper. Align with the base path (unavailable / nullopt at dist 0).

### [M] Escape-pod design failures fail closed without error
`src/game/units/BaseConquestEffects.cpp:121-171` — `EnsureEscapePodDesign_` returns `nullptr` when the registry is missing or any configured component id is unknown; `SpawnEscapePods_` then returns 0. Cross-species capture still strips population (`ApplySpeciesClashPopulation_`) but silently spawns no pods, violating the project preference to throw on unexpected null/config errors. Throw from ensure/spawn when `componentIds` is non-empty but assembly cannot proceed.

### [M] `risk_repeat` depends on a caller flag the executor never owns
`src/game/units/ProbeActionExecutor.cpp:94-96` / `156-165` — Repeat risk is selected only when the caller passes `bRepeatAtBase`. No in-slice state records prior missions at a base, so `risk_repeat` in `probe_actions.json` (e.g. steal tech) is inert unless every caller remembers. Track attempts on the target (or faction ledger) inside the probe pipeline, or drop the parameter and derive repeat from game state.

### [L] Convention and hygiene items
- `include/game/units/DisengageRules.h:22` — Header says “neither combatant is an air unit”; `DisengageRules.cpp:64-66` also blocks Orbital — update the contract comment.
- `include/game/units/ProbeActionConfig.h:35-68` — `ProbeActionIdToString` / `ParseProbeActionId` duplicate the snake_case map; keep one table.
- `src/game/units/ProbeActionEffects.cpp:118` — HQ exclusion uses string id `"Headquarters"` while `ProbeRules` already exposes `IsHeadquarters` via the rule flag.
- `src/game/units/AttackRules.cpp:5` / `ProbeActionEffects.cpp:9` — Likely unused includes (`EffectConfig.h`, `DiplomacyLedger.h`).
- `src/game/units/BaseConquestConfigParser.cpp:22-28` — Optional `json.value(..., default)` soft-fills tunables; other parsers in this slice throw on missing required structure.

**Observed outside slice:**
- `src/ui/world/WorldView.cpp:565-566` — `TryProbeAction` is invoked without `facilityId` or `bRepeatAtBase`, so facility sabotage and repeat-risk are unreachable from the current UI even if the executor API is fixed.
- `include/game/effects/EffectEnums.h` / chassis config — `DisengageChance` is a first-class stat with stock amounts; combat is the consumer that must honor it (finding above).

---

## Graphics backend

**Files:** `src/graphics/NullGraphics.cpp`, `src/graphics/SFMLGraphics.cpp`,
`include/graphics/Graphics.h`

**Assessment:** The `Graphics` interface is small and usable — UI code draws through
references to an abstract surface, and `SFMLGraphics` correctly owns its
`sf::RenderWindow` / font / texture map and throws if the window fails to open (prior
4.2 residue here is closed). The dominant weakness is that the SFML backend is not a
renderer: `Display()` is also the input pump, close-policy owner, and maximize hack, so
the declared Graphics/Input split is fiction. Secondary issues are silent degradation
(font load) and presentation constants that belong in config.

### [H] `Display()` secretly owns the input pipeline and window-close policy
`src/graphics/SFMLGraphics.cpp:131-135`, `223-268` — every frame, `Display()` calls
`ProcessEvents_()`, which drains the SFML event queue into file-scope
`PushPendingKeyEvent_t` / `PushPendingMouseEvent_t` globals and deliberately swallows
`sf::Event::Closed` ("only Enter should close"). Consequences: (1) `Graphics` and
`Input` are not interchangeable backends — `NullGraphics` never pushes, so any pairing
with `SFMLInput` yields dead input; (2) a render call has hidden I/O side effects that
preclude multiple windows and clean unit tests of drawing; (3) player-facing close
policy lives in the graphics TU instead of the engine/UI layer. Prior finding 4.6
recorded this and is still open; the graphics half of the fix is to stop pumping input
from `Display()` (hand the native window / poll loop to `Input`, or to a shared
window owner both backends receive).

### [M] Font load failure silently disables all text drawing
`src/graphics/SFMLGraphics.cpp:117-123`, `164-168` — if both hard-coded system font
paths fail, the constructor only logs to `stderr` and continues; `DrawText` then
returns early when `m_font.getInfo().family` is empty. The entire current UI is
text/rect based, so a missing distro font produces a black window with no labels and
no throw — opposite of the project rule to prefer exceptions over silent defaults.
Prior §9 noted this; still unfixed. Direction: throw from the constructor (or take a
configured font path and fail loud), and stop treating "no font" as a valid ready state.

### [M] Presentation settings are hard-coded in the SFML TU
`src/graphics/SFMLGraphics.cpp:36-39`, `83-84`, `89-92`, `113` — window size
(1280×900), title, 60 FPS cap, maximize wait policy, and two Debian-centric font paths
are compile-time literals with no config or constructor parameters. `NullGraphics`
duplicates the size constants (`src/graphics/NullGraphics.cpp:56-63`) so headless
layout math stays aligned only by copy. Prior §9; still open. Direction: pass a
`GraphicsConfig_t` (or equivalent) into `CreateGraphics` / the constructors so size,
title, FPS, and font path are data.

### [M] `NullGraphics` fails texture ops that a null object should no-op successfully
`src/graphics/NullGraphics.cpp:27-36` — `LoadTexture` / `DrawSprite` log and return
`false`. A substitutable null backend should accept loads and draws as successful
no-ops; returning failure forces every future caller to special-case headless mode and
breaks LSP relative to a backend that can satisfy the same calls. (No production caller
exists yet — both APIs are unused outside these files — but the contract is already
wrong.) Direction: return `true` from the null implementations (and consider throwing
from SFML on load failure per guidelines, instead of `bool`).

### [M] `LoadTexture` does not replace an existing id
`src/graphics/SFMLGraphics.cpp:145` — `m_textures.emplace(id, …)` leaves the previous
texture in place when `id` is already present, while still returning `true` after a
successful file load. Callers that reload/replace an asset will observe the old GPU
data with no error. Direction: `insert_or_assign` (or erase-then-emplace) after a
successful load.

### [L] Convention and hygiene items
- `src/graphics/NullGraphics.cpp:14-64` — method definitions are indented at column 0
  inside the class (brace/indent drift vs `SFMLGraphics`).
- `src/graphics/NullGraphics.cpp:39`, `src/graphics/SFMLGraphics.cpp:164` — default
  arguments repeated on overrides; only the base defaults in
  `include/graphics/Graphics.h:34-37` apply through a `Graphics*`.
- `src/graphics/SFMLGraphics.cpp:238` — local `auto KeyEvent_t` uses a type name as a
  variable (shadows the type; should be `pKeyEvent` / `keyPressed`).
- `src/graphics/SFMLGraphics.cpp:83-84` — `k_FontPath1` / `k_FontPath2` sit outside the
  anonymous namespace with external linkage in this TU; `SFMLGraphics` itself is also
  not hidden in an anonymous namespace (unlike `NullGraphics`).
- `src/graphics/NullGraphics.cpp:16` — logs "Null graphics backend" while prior review
  correctly noted the name oversells headless capability (console chatter, fixed size).
- `include/graphics/Graphics.h:17-22` — `Color_t` factories use same-line braces,
  contrary to the project brace rule.

**Observed outside slice:**
- `docs/architecture/graphics-system.md:7,63,73` — still documents `Initialize()`, an
  800×600 window, and `SFMLKeyEventQueue`; the code has no `Initialize`, defaults to
  1280×900, and pushes into `KeyEventQueue`/`MouseEventQueue`.
- `src/ui/ViewFactory.cpp:178-186` — fullscreen layout is snapshotted from
  `GetWindowWidth`/`Height` once; `SFMLGraphics` resize updates the SFML view
  (`SFMLGraphics.cpp:233-236`) but UI pixel layouts do not invalidate (prior 4.5).

---

## Input backend

**Files:** `src/input/KeyEventQueue.cpp`, `include/input/KeyEventQueue.h`,
`src/input/KeyMapping.cpp`, `include/input/KeyMapping.h`,
`src/input/MouseEventQueue.cpp`, `include/input/MouseEventQueue.h`,
`src/input/NullInput.cpp`, `src/input/SFMLInput.cpp`, `include/input/Input.h`

**Assessment:** The `Input` interface and the SFML/console split are small and readable, and
`KeyEvent_t` correctly carries modifiers with the keystroke. The dominant weakness is that
"input" is not a self-contained backend: pending events live in file-scope globals filled by
the graphics layer, so the abstract `Input`/`Graphics` pairing is a fiction. Secondary
issues are an `optional`-returning mapper that never returns empty, and an Async API that is
neither asynchronous nor substitutable across backends. Prior finding 4.6 is still open.

### [H] Own pending key/mouse events inside the input backend
`src/input/KeyEventQueue.cpp:7-11`, `src/input/MouseEventQueue.cpp:7-15`,
`src/input/SFMLInput.cpp:26-43` — events sit in process-global `static std::deque`s with free
`Push`/`Pop` functions; `SFMLInput` only drains them. Push sites are in
`SFMLGraphics::ProcessEvents_`, so pairing `NullGraphics` with `SFMLInput` yields a live
`Input` object that never sees a key or click. Globals also block multi-window use and force
tests to share one mutable queue. Same structural defect as prior finding 4.6 (still
unresolved). Direction: make the queues members of `SFMLInput` (or inject an event source
the graphics backend writes into), and stop exposing free push/pop as the integration seam.

### [H] `KeyFromSfKey` never returns `nullopt` — unmapped keys become `Unknown` events
`src/input/KeyMapping.cpp:209-210` — the default branch is `return Key_t::Unknown;`, so the
`std::optional<Key_t>` is always engaged. Callers that correctly write
`if (auto mapped = KeyFromSfKey(...))` (e.g. `SFMLGraphics.cpp:240-242`) therefore push a
`KeyEvent_t{Unknown, …}` for every unmapped key (Tab, Backspace, punctuation, etc.).
`KeyFromAscii` and `MouseButtonFromSfButton` return `nullopt` for unknowns; this mapper
lies about the same contract. Direction: `return std::nullopt` in the default branch (and
never push `Unknown` unless a real unknown key event is intentional).

### [M] `CaptureKey` discards modifiers that `CaptureKeyAsync` preserves
`include/input/Input.h:68-69`, `src/input/SFMLInput.cpp:24-38` — `CaptureKey` returns
`optional<Key_t>` after popping a full `KeyEvent_t` and throwing away `modifier`, while
`CaptureKeyAsync` delivers the whole event. `KeyEvent_t`'s own comment
(`include/input/Input.h:41-43`) says consumers must read modifiers from the event rather
than polling. Any caller that needs chords is forced onto the misnamed Async API; any caller
that uses `CaptureKey` silently loses Ctrl/Alt/Shift. Direction: make `CaptureKey` return
`optional<KeyEvent_t>` (mirror `CaptureMouse`) and drop the key-only overload.

### [M] `*Async` methods are synchronous and not substitutable across backends
`src/input/SFMLInput.cpp:33-38`, `src/input/NullInput.cpp:30-39` — neither implementation
schedules work; both run inline. Worse, behavior diverges under LSP: SFML invokes the
callback only when a pending event exists; Null always blocks on `stdin` and still invokes
the callback with `Key_t::Unknown` / `MouseButton_t::None` on failure
(`NullInput.cpp:36-38`, `76-78`). A loop written against SFML's "no callback means empty"
semantics misbehaves under Null, and vice versa. Direction: rename to try/poll semantics
(or drain-all with a real queue on the object), and make both backends agree on
callback-vs-empty rules — Null should not synthesize fake events.

### [M] `NullInput` is a blocking console backend, not a null/headless object
`src/input/NullInput.cpp:18-27`, `42-49` — `CaptureKey`/`CaptureMouse` print prompts and
block on `std::cin`. Named and factory-selected as the non-SFML `Input`, this stalls any
frame loop that polls input every tick; it is unsuitable for automated/headless runs.
Direction: rename to `ConsoleInput` (matching its own log line) and provide a true no-op
`Input` that returns empty optionals without blocking, if headless is a real requirement.

### [M] Hand-rolled key maps drift; `Key_tToString` omits F1–F12
`src/input/KeyMapping.cpp:98-146` vs `150-210` — three parallel switches
(`KeyFromAscii`/`KeyToAscii`/`Key_tToString`/`KeyFromSfKey`) must be edited together when
`Key_t` grows. `Key_tToString` has no cases for `F1`–`F12` (defined at
`include/input/Input.h:19`) and falls through to `"Unknown"`, while `KeyFromSfKey` maps
those keys. Guidelines prefer `magic_enum` when the string form matches the enumerator;
`Key_tToString` is exactly that case. Direction: implement `Key_tToString` via
`magic_enum::enum_name` (keep explicit maps only for ASCII/SFML wire forms).

### [L] Convention and hygiene items
- `include/input/KeyEventQueue.h:9`, `include/input/MouseEventQueue.h:15` —
  `PushPendingKeyEvent_t` / `PushPendingMouseEvent_t` put the `_t` type suffix on functions
  (called out in prior 4.6; still present).
- `include/input/KeyMapping.h:17` — `Key_tToString` mangles a type suffix into a function
  name; prefer `KeyToString`.
- `include/input/Input.h:23-29` — `Modifier_t` is unused; only `ModifierState_t` is live.
- `include/input/Input.h:69`, `include/input/KeyMapping.h:15,19`,
  `src/input/SFMLInput.cpp:24` — missing space before `CaptureKey` / `KeyFromAscii` /
  `KeyFromSfKey` declarators (`std::optional<Key_t>CaptureKey`).
- `src/input/KeyMapping.cpp:7-49` — switch bodies are column-0 indented; inconsistent with
  project brace/indent style.
- `src/input/SFMLInput.cpp:7-12` — unused includes (`chrono`, `thread`, `vector`, SFML
  Keyboard/Mouse) left after the old polling `CaptureMouse` was removed.
- `include/input/MouseEventQueue.h:18`, `src/input/MouseEventQueue.cpp:23-25` —
  `GetLastMousePosition` returns `{0,0}` when never set instead of throwing; callers must
  remember `HasLastMousePosition` first.
- No tests under `tests/` exercise queues, mapping, or either backend for implemented
  poll/drain behavior.

**Observed outside slice:**
- `src/ui/UIManager.cpp:42-52` vs `74-83` — `ProcessKeys_` invokes `CaptureKeyAsync` once
  per frame (at most one key) while `ProcessMouse_` drains the full mouse queue; fast typing
  can backlog in the global key deque indefinitely.
- `src/graphics/SFMLGraphics.cpp:228-231` — window close is swallowed inside the graphics
  backend ("only Enter should close"); interaction policy buried outside Input.
- `docs/architecture/input-system.md` — still documents `Initialize()`, a blocking
  SFML `CaptureMouse` poll loop, and `SFMLKeyEventQueue` naming that no longer match the
  code.

---

## Shared libraries

**Files:** `src/lib/EventBus.cpp`, `include/lib/EventBus.h`, `src/lib/LuaRuntime.cpp`, `include/lib/LuaRuntime.h`, `src/lib/Rational.cpp`, `include/lib/Rational.h`, `src/lib/config/ConfigFields.cpp`, `include/lib/config/ConfigFields.h`, `include/lib/Signal.h`, `include/lib/GameEvent.h`, `include/lib/IdAllocator.h`, `include/lib/RandomRoll.h`, `include/lib/DerefView.h`, `include/lib/Revision.h`, `include/lib/Registry.h`, `include/lib/config/JsonConfigLoader.h`

**Assessment:** The small utilities (`Revision`, `IdAllocator`, `RandomRoll`, `DerefView`, `JsonConfigLoader`, `ConfigFields` helpers) are clear and appropriately thin. `Signal` is in good shape after `Disconnect` / `ScopedConnection` / emit-time snapshotting, with solid unit coverage. The dominant weaknesses are the still-unsafe mod-facing `EventBus` dispatch path and `LuaRuntime::EvalInt`'s warn-and-zero / leaked-globals contract, both of which contradict project error-handling rules in the most extension-facing APIs.

### [H] Snapshot handlers in `EventBus::Publish` (reentrancy UB)
`src/lib/EventBus.cpp:23-26` — `Publish` iterates `m_handlers` directly. A handler that `Subscribe`s (vector reallocation) or `Unsubscribe`s (erase) during dispatch invalidates that iteration — undefined behavior on the bus documented as the mod-facing ABI (`docs/architecture/event-system.md`). `Signal::Emit` already snapshots and skips disconnected slots (`include/lib/Signal.h:107-117`); apply the same pattern here. This is the incomplete half of prior finding 1.6 (`Signal` side was fixed; `EventBus` was not). No `EventBus` reentrancy tests exist alongside `tests/lib/SignalTests.cpp`.

### [M] Make `LuaRuntime::EvalInt` fail loudly and isolate variables
`src/lib/LuaRuntime.cpp:37-70` — On any formula error the method logs to `stdout` and returns `0`; an empty formula returns `0` with no warning (`40-43`). Variables are written as persistent globals (`46-49`) and never cleared, despite the comment claiming they are "scoped to this call," so a missing input in formula B silently reads formula A's stale value. Callers use this for tech cost and pop composition (`TechCostCalculator.cpp:34`, `PopCompositionCalculator.cpp:29-30`), so typos become wrong game numbers instead of load/eval failures. Also, `static_cast<int>(result.get<lua_Number>())` (`63`) truncates non-integer results without complaint. Prefer throw-on-error (per guidelines and the 3.8 rule explicitly leaving this open under 3.6), evaluate in a fresh environment or nil-out keys after the call, and reject non-integral results. Prior finding 3.6 — still unresolved.

### [M] Keep `Registry::Load` atomic across validation failure
`include/lib/Registry.h:29-42` — `Load` replaces `m_configs` / `m_indexById` before `Validate_()`. If `ValidateNoDuplicates_` (or a subclass override) throws, the previous good registry contents are already gone and the object is left holding the rejected payload. Build the new index in locals, validate, then commit — or restore the prior vectors on failure — so a bad config file cannot destroy a previously loaded registry.

### [M] Reject wrong-typed arrays in `ParseStringArray`
`src/lib/config/ConfigFields.cpp:23-35` — When `key` is present but not a JSON array, the function returns `{}` with no error, same as a missing key. Mod authors who write `"prerequisites": "tech_x"` (or an object) get a silent empty list instead of a parse failure. Other loaders in this slice throw on shape errors (`JsonConfigLoader::LoadFile` at `include/lib/config/JsonConfigLoader.h:39-43`). If the key exists, require `is_array()` and throw otherwise.

### [M] Harden `Rational_t::ScaledInt` against overflow
`src/lib/Rational.cpp:59-73` — After reducing, the return is `num * (scale / den)` in `int`. Large numerators or scales invoke signed overflow (UB) before any divisibility check can help. Compute with a wider intermediate (e.g. `int64_t`) and throw if the product does not fit in `int`, matching the existing "must scale exactly" failure style.

### [L] Convention and hygiene items
- `include/lib/GameEvent.h:9-11` — `FactionId_t` / `BaseId_t` are re-declared here while `BaseTypes.h` also defines `FactionId_t`; `TechId` omits the `_t` suffix used by the sibling aliases (accepted layering tradeoff from prior 1.2, but the naming split remains).
- `include/lib/Registry.h:84-101` — `ValidateNoDuplicates_` is O(n²); collisions are already visible while filling `m_indexById` (prior §9 item, still true).
- `include/lib/LuaRuntime.h:25-26` / `src/lib/LuaRuntime.cpp:59,67` — Header documents warn-and-return-0; that contract itself fights the project throw-on-error rule once 3.6 is fixed.
- `include/lib/DerefView.h:16,22` — Transforms dereference `unique_ptr` with no null check; a null element is hard UB versus the guideline to throw on unexpected null.
- `include/lib/EventBus.h:34` vs `include/lib/Signal.h:130` — Subscription ids start at `0` on the bus and `1` on signals (`0` reserved); harmless but inconsistent.
- `include/lib/Signal.h` — Architecture doc still claims "zero heap allocation" for signals (`docs/architecture/event-system.md:61-62`) while the implementation stores `std::function` slots; update the doc when diagrams are next touched.

**Observed outside slice:**
- `docs/architecture/event-system.md` — Still describes snake_case signal names and "zero heap" `Signal` behavior that no longer matches `include/lib/Signal.h`.
- Prior 1.6 lifetime/wiring issues in `EventBridge` / `BaseManager` signal graphs remain caller-side; fix sites are outside this file list.

---

## UI — manager, views, tile rendering

**Files:** `src/ui/PlaceholderPanel.cpp`, `include/ui/PlaceholderPanel.h`, `src/ui/TileHitTester.cpp`, `include/ui/TileHitTester.h`, `src/ui/TileRenderer.cpp`, `include/ui/TileRenderer.h`, `src/ui/UIManager.cpp`, `include/ui/UIManager.h`, `src/ui/ViewFactory.cpp`, `include/ui/ViewFactory.h`, `include/ui/IGameView.h`, `include/ui/UIElement.h`

**Assessment:** `UIManager` / `IGameView` / `UIElement` form a small, readable stack with clear ownership (`unique_ptr` views and elements). `TileRenderer` correctly pulls colors from `UiStyle`, and `PlaceholderPanel` is an honest stub. The dominant weaknesses are input/stack policy gaps in `UIManager`, null-tolerant factory returns that fight project guidelines, and a stale `TileHitTester` workable-area API that no longer matches its callers.

### [H] Block global shortcuts from stacking overlays
`src/ui/UIManager.cpp:42-72` — `ProcessKeys_` falls through to `HandleGlobalShortcut_` whenever the active view returns `false` from `HandleKey`. Overlay views only consume `Escape` (e.g. research/settings/base), so repeated `F2`/`E`/`U`/… pushes another full-screen view each time. `CombatView` already documents this hazard by swallowing every key (`src/ui/world/CombatView.cpp:77-81`); the fix belongs in `UIManager` (ignore shortcuts while an overlay is active, or replace/dedupe by kind) rather than in each view.

### [H] Do not return null views or push unchecked pointers
`src/ui/ViewFactory.cpp:63-67,77-80,90-93,108-111` — several `Create*View` methods return `nullptr` when `GetPlayerFaction()` is missing, against “prefer throwing / throw on unexpected null.” `UIManager::PushView` (`src/ui/UIManager.cpp:109-112`) unconditionally calls `pView->OnPushed` with no null check. Shortcut factories null-check before push (`UIManager.cpp:68-71`), but open-base and similar callers can pass a null `unique_ptr` straight in. Throw from the factory (and/or `PushView`) so missing player state fails loudly.

### [M] Stop culling closed views/elements inside Render
`include/ui/IGameView.h:21-28` and `src/ui/UIManager.cpp:94-101` — element and overlay lifetime is mutated during `Render` (`ShouldClose` → erase / `OnPopped`). Until the next render, a closed overlay remains `GetActiveView_()` and still receives further mouse events drained in the same `ProcessMouse_` loop (`UIManager.cpp:74-83`). Prior review 4.3 deferred renaming `IGameView` and moving prune out of `Render`; the incomplete fix still leaves input routed at a dying top view. Prune (or pop) closed views at the start/end of `ProcessInput`, not inside paint.

### [M] Remove or rewire dead workable-area hit test
`include/ui/TileHitTester.h:10,23-36` / `src/ui/TileHitTester.cpp:49-81` — `HitTestBaseWorkableArea` has no callers; `BaseWorkableAreaDisplay` hit-tests its own cached rects. The header still claims shared use with that display. The helper hardcodes `k_WorkableGridRadius = 2` separately from `MapUtils`’s `k_WorkableRadius`, excludes the origin, and returns unwrapped `baseX+dx` (model iteration wraps via `WorldMap::GetTile`). Delete it, or implement one shared path that calls `InEuclideanRadius` / workable helpers so UI and rules cannot drift.

### [M] Clarify HitTestWorldGrid contract
`include/ui/TileHitTester.h:14-21` — comments say “full world map” and “world tile coordinates,” but the function returns origin-relative grid indices for whatever width/height the caller passes. `WorldView` correctly feeds the visible viewport size and then converts via `WorldCoordsAt`. Rename parameters/docs (e.g. `gridCols`/`gridRows`, “grid indices”) so the next caller does not treat the result as world coordinates.

### [M] Stop hardcoding the Forest improvement id in TileRenderer
`src/ui/TileRenderer.cpp:102-104` — fill color keys off `HasImprovement("Forest")`. Forest is a config id (`config/improvements.json`); a mod rename or alternate forest-like improvement silently loses the overlay color. Drive the special-case id (or a style flag) from config/`UiStyle`, same as the other tile colors.

### [M] Narrow ViewFactory’s header dependencies
`include/ui/ViewFactory.h:5-14` — the factory header includes every concrete view type. Any TU that needs `ViewFactory` recompiles when any view changes, and the factory surface depends on concretions rather than `IGameView`. Forward-declare return types in the header (or return `unique_ptr<IGameView>` where practical) and include concrete views only in `ViewFactory.cpp`.

### [L] Convention and hygiene items
- `include/ui/IGameView.h:13` — concrete base with state named like an interface; prior 4.3 deferred rename to `GameView`.
- `include/ui/UIElement.h:61-64` — `Contains` duplicates `ContainsMouseCoord` on the same `Rectangle_t` shape.
- `include/ui/UIElement.h:37-38` — `ResolveLayout` rejects ratios `> 1` but allows negatives.
- `include/ui/UIManager.h:17,44` — uses `std::function` / `std::unordered_map` without including `<functional>` / `<unordered_map>`; `ViewFactory_t` aliases collide conceptually with class `ViewFactory`.
- `src/ui/TileHitTester.cpp:74-80` — `IsInWorkableDiamond_` names a Euclidean disk; `k_BaseCenterOffset = 0` is noise for “exclude origin.”
- `src/ui/TileRenderer.cpp:25-50` — `MoistureToInt_` / `RockinessToInt_` use `default:` → `0` instead of throwing on unexpected enumerators.
- `src/ui/ViewFactory.cpp:83` — `ResearchView` still takes `const ResearchManager*` rather than a reference.

**Observed outside slice:**
- `src/game/Engine.cpp:418-468` — shortcuts capture `GetFullscreenLayout()` once at init (prior 4.5 resize/layout drift still open); open-base lambda pushes `CreateBaseView` with no null check.
- `docs/architecture/ui-system.md` — still describes `UIManagerImpl`, `UIWorldMap`/`UIPanel`/`UIPopup`, and `IGameView::Update`; does not match this slice.
- `src/ui/base/BaseWorkableAreaDisplay.cpp:106-136` — owns hit-testing that made `HitTestBaseWorkableArea` dead (fix lives there if the shared helper is restored).

---

## UI — base view and displays

**Files:** `src/ui/base/BaseNameDisplay.cpp`, `include/ui/base/BaseNameDisplay.h`, `src/ui/base/BaseView.cpp`, `include/ui/base/BaseView.h`, `src/ui/base/BaseWorkableAreaDisplay.cpp`, `include/ui/base/BaseWorkableAreaDisplay.h`, `src/ui/base/GrowthDisplay.cpp`, `include/ui/base/GrowthDisplay.h`, `src/ui/base/PopulationDisplay.cpp`, `include/ui/base/PopulationDisplay.h`, `src/ui/base/ProductionDisplay.cpp`, `include/ui/base/ProductionDisplay.h`, `src/ui/base/SupportDisplay.cpp`, `include/ui/base/SupportDisplay.h`

**Assessment:** The slice is a thin coordinator (`BaseView`) plus focused read-only panels that pull layout/colors from `UiStyle` and stay easy to scan. The dominant weaknesses are interaction design (non-modal in-view popups), constructors that accept nullable pointers the caller always has as references, and presentation rules (pop glyphs) hard-coded in render code instead of config.

### [M] Make in-view selector popups modal
`src/ui/base/BaseView.cpp:206-212` / `225-238` — `HandlePopClick` / `HandleProductionDisplayClicked_` push `PopTypeSelectorPopup` / `ProductionSelectorPopup` into `m_elements`, but `BaseView` does not override `HandleMouse`. `IGameView::HandleMouse` delivers the click only to the topmost element whose `Contains` is true; both popups use partial layouts (`popupSmall` / `topPanel`), so clicks on tiles, pops, or production still reach the underlying panels, can mutate assignments, and can stack a second popup. Fix: override `HandleMouse`/`HandleKey` to consume input while any popup child is open (or insert a fullscreen dismiss layer), and dismiss/replace an existing selector before opening another.

### [M] Drop the separate `Faction&`; use the base’s owner
`include/ui/base/BaseView.h:24` / `43`, `src/ui/base/BaseView.cpp:207` — `BaseView` stores `m_rFaction` only for `GetAvailablePopTypes()`, while `BaseManager::GetFaction()` already exposes the owning faction. `ViewFactory` always passes the player faction even when `bEditable` is false for a foreign base, so the view carries a faction reference that can disagree with `m_rBase`. Fix: remove the `Faction` constructor parameter and call `m_rBase.GetFaction().GetAvailablePopTypes()`.

### [M] Require non-null base/population at construction (use references)
`include/ui/base/GrowthDisplay.h:15`, `ProductionDisplay.h:16`, `BaseNameDisplay.h:15`, `SupportDisplay.h:15`, `BaseWorkableAreaDisplay.h:25`, `PopulationDisplay.h:20` — every panel takes a nullable `const BaseManager*` / `PopulationManager*` though `BaseView` always passes a live object. Most panels only throw on null inside `Render` (`GrowthDisplay.cpp:22-25`, etc.), so a null digs in as a latent invalid object. Guidelines require valid construction and prefer references. Fix: take `const BaseManager&` / `PopulationManager&` (throw in the ctor if a pointer API must remain). Related to prior review §6 pointer→reference cleanup (still open).

### [M] Hard-coded pop display glyphs belong in config
`src/ui/base/PopulationDisplay.cpp:111-122` — letter labels use the first character of the type id, then special-case `IsDrone()` → `'R'` and `IsTalent()` → `'A'` to dodge Drone/Doctor and Talent/Technician collisions. Any new modded type that shares an initial (or a non-drone with `riot_contribution > 0`) silently gets a wrong or colliding glyph; `PopTypeConfig_t` has no display-letter field. Fix: add an explicit glyph (or icon id) on pop-type config and render that.

### [M] Panels still force full live yield/production work every frame
`src/ui/base/BaseWorkableAreaDisplay.cpp:71-72` / `87-89`, `GrowthDisplay.cpp:46-51`, `ProductionDisplay.cpp:56-66` — each frame re-queries `GetWorkedTileYield` / `GetPreviewTileYield` per workable tile and nutrient/mineral production getters. Pool filtering is memoized (prior §1.1 fixed), but `ResourceManager::ComputeWorked_` memoization and `CollectAreaEffects` cost remain deferred there; these UI call sites are still the hot drivers with no display-side revision stamp. Fix: render from a per-frame or revision-keyed snapshot (or skip re-query when worker/effects revisions are unchanged).

### [M] Player actions mutate managers with no command/event seam
`src/ui/base/BaseView.cpp:176-196`, `222`, `237` — tile assign/unassign, reset-all, `ConvertPop`, and `SetProduction` are direct manager calls from UI handlers. Mods cannot observe or intercept these actions (prior §1.10, still open). Fix direction lives partly here: route through a command/action API that emits events before touching `BaseManager`.

### [L] Convention and hygiene items
- `include/ui/base/BaseView.h:37-38` — `HandlePopClick` / `HandlePopTypeSelected` are private but lack the trailing `_` used by `HandleTileClick_` / `HandleBaseClicked_` (prior §6 called this out; still true).
- `include/ui/base/BaseWorkableAreaDisplay.h:14` — comment says “21 tiles”; workable set is the 20-tile ring and the center is drawn separately (`BaseWorkableAreaDisplay.cpp:75-78`; see `MapUtils.h` radius-2 note).
- `include/ui/base/PopulationDisplay.h:3` — includes `ui/IGameView.h` though the class only needs `UIElement` (and `<vector>` for `m_popBoxes` is missing, relying on transitive includes).
- `src/ui/base/GrowthDisplay.cpp` / `ProductionDisplay.cpp` — nearly identical panel chrome/line layout; a tiny shared helper would remove drift risk.
- `src/ui/base/SupportDisplay.cpp:52-55` — units that do not fit are dropped with `break` and no overflow cue.
- No tests under `tests/` exercise these displays or `BaseView` input (pop convert, tile assign, production click, editable gating).

**Observed outside slice:**
- `docs/architecture/ui-system.md` / `high-level.md` still describe `IBasePanel` / `BaseDisplay`; neither exists — live types are `UIElement` subclasses including `BaseNameDisplay`, `ProductionDisplay`, `SupportDisplay`.
- `src/ui/base/ProductionSelectorPopup.cpp:92-96` — outside-click dismiss never runs: `IGameView::HandleMouse` only invokes `HandleMouseClick` when `Contains` is true.
- `src/ui/ViewFactory.cpp:69-70` — always passes the player `Faction` into `BaseView` (feeds the dual-faction issue above).

---

## UI — base selector popups

**Files:** `src/ui/base/PopTypeSelectorPopup.cpp`, `include/ui/base/PopTypeSelectorPopup.h`, `src/ui/base/ProductionSelectorPopup.cpp`, `include/ui/base/ProductionSelectorPopup.h`

**Assessment:** Both popups are small, readable list pickers that pull chrome from `UiStyle` and correctly dismiss on Escape. The dominant weakness is that they are near-copies of each other (and of several other selector popups) that have already diverged in dismiss and null handling, with no clipping/scroll for long lists and no real modal hit-testing. Nothing here is large or clever; the risk is silent UX breakage as constructable/pop-type lists grow.

### [H] Extract the duplicated list-selector instead of letting copies diverge
`src/ui/base/PopTypeSelectorPopup.cpp` and `src/ui/base/ProductionSelectorPopup.cpp` are the same widget (cache entry rects → draw header/list → Escape closes → click invokes callback) with different payload types and copy strings. They have already drifted: Production closes on “outside” click and soft-skips null entries (`ProductionSelectorPopup.cpp:92-106`); PopType neither dismisses outside nor null-checks before deref (`PopTypeSelectorPopup.cpp:95-112`). Further selector copies in the tree (`ComponentSelectorPopup`, `CouncilProposalsPopup`, `SupplyCrawlPopup`, …) show the same fork. Collapse to one typed/list-callback helper (or shared private base) so dismiss, null, and layout rules have a single owner.

### [M] Outside-click dismiss in ProductionSelector is unreachable
`src/ui/base/ProductionSelectorPopup.cpp:92-96` sets `m_bShouldClose` when the click is outside `m_layout`, but `IGameView::HandleMouse` only calls `HandleMouseClick` after `Contains` on that same layout (`include/ui/IGameView.h:55-61`). The branch never runs under normal routing, so maintainers inherit a false contract (“click outside closes”) while clicks outside the popup hit underlying BaseView panels and can open a second selector. Remove the dead branch until modal routing exists, or give the popup a fullscreen hit target with inset chrome.

### [M] Long lists overflow the popup with no clip or scroll
`CacheEntryRects_` walks the full vector with a fixed `lineHeightRatio` and never clamps to `m_layout.height` (`PopTypeSelectorPopup.cpp:22-37`, `ProductionSelectorPopup.cpp:21-36`). With `header_line_offset: 2` and `line_height_ratio: 0.05` that is roughly eighteen visible rows; further entries paint past the chrome and fall outside `Contains`, so they are neither reliably visible nor clickable. Bound the laid-out rows to the content area and add scroll (or reject overflow loudly) before buildings/projects/units push past that limit.

### [M] Click on a row can no-op when callback is empty or pointer is null
Both handlers only close after a successful callback invoke; Production also requires a non-null item (`PopTypeSelectorPopup.cpp:105-110`, `ProductionSelectorPopup.cpp:102-106`). An empty `std::function` or a null `IConstructable*` leaves the popup open with no feedback, against the project rules to throw on unexpected null and to construct only valid objects. Validate non-empty callback (and non-null entries) in the constructor or on click; always dismiss after a hit row is resolved.

### [L] Convention and hygiene items
- `include/ui/base/ProductionSelectorPopup.h:15-16` — comment says “lists buildings” but the type is `IConstructable` (any constructable).
- `include/ui/base/ProductionSelectorPopup.h:7` — unused `#include <string>`.
- `include/ui/base/PopTypeSelectorPopup.h:15` / `ProductionSelectorPopup.h:17` — architecture doc still calls these `UIPopup`; both inherit `UIElement` (no `UIPopup` type exists).
- No tests under `tests/` exercise either popup’s hit-testing, Escape, or empty-list rendering.

**Observed outside slice:**
- `include/ui/IGameView.h:48-62` — mouse routing is hit-test-only; open selectors do not capture outside clicks, so BaseView panels keep receiving input and can stack another popup.
- `docs/architecture/ui-system.md:40-95` — diagram lists only `PopTypeSelectorPopup` (as `UIPopup`) and omits `ProductionSelectorPopup`.

---

## UI — commlinks

**Files:** `src/ui/commlinks/CommlinksPanel.cpp`, `include/ui/commlinks/CommlinksPanel.h`, `src/ui/commlinks/CommlinksView.cpp`, `include/ui/commlinks/CommlinksView.h`, `src/ui/commlinks/CouncilButton.cpp`, `include/ui/commlinks/CouncilButton.h`, `src/ui/commlinks/CouncilCooldownPopup.cpp`, `include/ui/commlinks/CouncilCooldownPopup.h`, `src/ui/commlinks/CouncilProposalsPopup.cpp`, `include/ui/commlinks/CouncilProposalsPopup.h`

**Assessment:** Thin, readable overlay: panel lists known factions, view owns council open/propose flow, popups mirror other selector UIs. The dominant weakness is fragile modal layering and silent failure paths in `CommlinksView` — proposal UI can stack, dismiss-outside never fires under current hit-testing, and missing-commlinks / pending-vote rejects close the list with no player feedback (unlike the cooldown path).

### [H] Prevent stacking multiple council proposal popups
`src/ui/commlinks/CommlinksView.cpp:69-89` — `OpenCouncilProposals_` always `push_back`s a new `CouncilProposalsPopup` with no “already open” guard. With `config/ui/style.json` layouts, `top_panel` ends at view Y ratio `0.65` while the Council button (under `popup_small` + `council_button_layout`) starts at `0.652`, so `IGameView::HandleMouse` still hits `CouncilButton` while a proposals popup is up and opens another copy. Guard before push, or make the proposals UI a full-window modal hit target that covers the button.

### [M] Do not silently drop proposal selection on gated failures
`src/ui/commlinks/CommlinksView.cpp:92-109` — After the list entry is chosen, `CouncilProposalsPopup` closes itself (`CouncilProposalsPopup.cpp:115-118`). `OnProposalSelected_` then returns with no UI when `GetPending()` is set, or when `HasCommlinksToAllMembers` fails. Cooldown correctly opens `CouncilCooldownPopup_`; the other propose-time gates leave the player with a vanished list and no explanation. Surface a notice (or keep the list open) for pending and missing-commlinks the same way cooldown is handled.

### [M] Outside-click dismiss on proposals popup is unreachable
`src/ui/commlinks/CouncilProposalsPopup.cpp:105-108` — Click-outside sets `m_bShouldClose`, but `IGameView::HandleMouse` only calls `HandleMouseClick` when the cursor is inside the element’s layout (`include/ui/IGameView.h:55-62`), so that branch never runs. Escape works; the written dismiss path is dead. Use a fullscreen modal layout so outside chrome is still this element, or delete the dead branch and treat Escape as the only dismiss.

### [M] Wire or drop unused `playerCooldownYears`
`include/ui/commlinks/CouncilCooldownPopup.h:19` / `src/ui/commlinks/CouncilCooldownPopup.cpp:23` — `ProposeCooldownYears` is passed in (`CommlinksView.cpp:63`) and stored as `m_playerCooldownYears`, but `Render` never draws it; the body shows member/governor intervals and years remaining only. Callers look like they supply player-facing cooldown data that is discarded — render “your cooldown” or remove the parameter.

### [L] Convention and hygiene items
- `include/ui/commlinks/CouncilButton.h:4` — unnecessary `#include "graphics/Graphics.h"` (forward declare / cpp-only).
- `src/ui/commlinks/CouncilButton.cpp:27-32` — `HandleMouseClick` does not require `MouseButton_t::Left` (unlike the two council popups).
- `src/ui/commlinks/CouncilCooldownPopup.cpp:33` / `CouncilProposalsPopup.cpp:24` — both restyle from `Style().productionSelectorPopup`, so production UI tweaks silently restyle council chrome; prefer a dedicated style block.
- `src/ui/commlinks/CouncilCooldownPopup.cpp:36-37` — OK button size uses hardcoded `0.35f` / `1.4f` instead of style/config.
- `src/ui/commlinks/CommlinksPanel.cpp:33-37` / `CommlinksView.cpp:52-55` — null `GetPlayerFaction()` / council returns quietly; guidelines prefer throw when a player-facing view requires them.
- `include/ui/commlinks/CouncilCooldownPopup.h:11` — comment says popup shows when opening council on cooldown; it actually opens after selecting a proposal while on cooldown (`CommlinksView.cpp:100-103`).

**Observed outside slice:**
- `include/ui/IGameView.h:55-62` — hit-test-before-click makes every popup’s “click outside layout to close” pattern dead unless the element is fullscreen.
- `docs/architecture/ui-system.md` — omits `CommlinksView` / council propose UI entirely (architecting rule expects diagram updates for new components).

---

## UI — council vote

**Files:** `src/ui/council/CouncilBallotPopup.cpp`, `include/ui/council/CouncilBallotPopup.h`,
`src/ui/council/CouncilFactionVotesPanel.cpp`, `include/ui/council/CouncilFactionVotesPanel.h`,
`src/ui/council/CouncilProposalInfoPanel.cpp`, `include/ui/council/CouncilProposalInfoPanel.h`,
`src/ui/council/CouncilVoteButton.cpp`, `include/ui/council/CouncilVoteButton.h`,
`src/ui/council/CouncilVoteView.cpp`, `include/ui/council/CouncilVoteView.h`

**Assessment:** The view is a clear coordinator — top member columns, center proposal/tally,
Vote button, and a mode-specific ballot popup — and it correctly resolves only when
`AllMembersVoted()`. Layout and colors for the panels come from `councilVoteView` style. The
dominant weakness is lifecycle: Escape (and silent no-ops) can leave a pending proposal with no
UI re-entry, and election candidates / per-frame weight work are not tied to the council’s own
APIs (`GovernorCandidates`, `GetRevision`).

### [H] Do not dismiss the vote view while a proposal is still pending
`src/ui/council/CouncilVoteView.cpp:37-40` — Escape sets `m_bShouldClose` without casting or
calling `Resolve`. With AI ballots cast on `OnProposalOpened` (`Engine.cpp:335-340` →
`CastStubCouncilVotes`), only the player remains; if they Escape before voting, `m_pending`
stays set. `CommlinksView::OnProposalSelected_` refuses a new `Propose` while pending
(`src/ui/commlinks/CommlinksView.cpp:96-98`), and `CreateCouncilVoteView` is only pushed after a
successful Propose — so the council is bricked for the rest of the game. Fix: refuse Escape
(or treat it as abstain + resolve) until there is no pending proposal; do not close the overlay
on an open vote.

### [M] Election ballot lists every member, not eligible governor candidates
`src/ui/council/CouncilVoteView.cpp:69-73` — `CreateElection` always gets `pCouncil->Members()`.
For `electionOutcome == PlanetaryGovernor`, architecture and `GovernorCandidates()` restrict the
field to the two most populous members; `CastElectionVote` currently does not enforce that
either (sibling council-runtime finding). The UI is the surface that decides what the player can
pick, so ineligible winners are one click away. Pass `GovernorCandidates()` (or a council helper
keyed on `electionOutcome`) instead of the full membership for governor elections.

### [M] Recompute vote weights every frame with no revision cache
`src/ui/council/CouncilFactionVotesPanel.cpp:119` and
`src/ui/council/CouncilProposalInfoPanel.cpp:71,121` — both `Render` paths call
`ComputeVoteWeight` per member every frame. That copies the faction effect pool and runs
`ResolveStatModifiers` (`PlanetaryCouncil.cpp:163-175`). The council already exposes
`GetRevision()` for pull-based UI, but neither panel caches weights/tallies against it. Cache
display state on council revision (and invalidate when ballots change) instead of resolving
stats on paint.

### [M] Vote can stack multiple ballot popups
`src/ui/council/CouncilVoteView.cpp:26-28,71,86` — each Vote click `push_back`s a new
`CouncilBallotPopup`. The popup uses `popupSmall` and does not cover the Vote button; hit-test
is topmost-first (`IGameView::HandleMouse`), so a click on Vote while a popup is open adds
another chooser. Guard with a single open selector (or disable Vote while one exists).

### [M] Silent no-ops when council or player is missing
`src/ui/council/CouncilVoteView.cpp:47-50,58-62,77-80,91-94` — `TryResolveAndClose_`,
`OpenBallotSelector_`, and the cast lambdas return quietly if `GetPlanetaryCouncil()` or
`GetPlayerFaction()` is null. This view only exists for an active council vote; those pointers
being null is unexpected. Per project guidelines, throw rather than leave the player staring at
a Vote button that does nothing. Same pattern in the panel `Render` early returns
(`CouncilFactionVotesPanel.cpp:83-91`, `CouncilProposalInfoPanel.cpp:31-39`) — empty chrome with
no diagnostic.

### [M] `Resolve` from the cast callback is uncaught
`src/ui/council/CouncilVoteView.cpp:52,80,94` — `Resolve` throws if preconditions fail (e.g. not
all members voted). The callback has no try/catch, so a council invariant failure becomes an
unhandled exception out of the input/render loop. Catch and surface, or assert the
`AllMembersVoted()` gate so a throw is truly unreachable.

### [L] Convention and hygiene items
- `src/ui/council/CouncilFactionVotesPanel.cpp:17-28` — `BallotLabel_` hand-rolls Yea/Nay/Abstain; enumerator names match display strings, so prefer `magic_enum` (same labels duplicated at `CouncilBallotPopup.cpp:40-41`).
- `src/ui/council/CouncilBallotPopup.cpp:81,103` — ballot chrome uses `Style().productionSelectorPopup` instead of a council-specific style block.
- `src/ui/council/CouncilVoteButton.cpp:16` — Vote button uses `Style().commlinksButton` while panels use `councilVoteView`.
- `src/ui/council/CouncilFactionVotesPanel.cpp:109` / `CouncilProposalInfoPanel.cpp:67` — null-member guards are dead; `PlanetaryCouncil` rejects null members at construction.
- No tests exercise Escape-abandon, election candidate filtering, or the vote→resolve UI path (game tests cover council rules only).

**Observed outside slice:**
`src/ui/commlinks/CommlinksView.cpp:96-116` — only entry to `CouncilVoteView` is post-Propose; no path reopens an existing pending vote (amplifies the Escape finding).
`src/game/council/PlanetaryCouncil.cpp:353-357` — `CastElectionVote` accepts any member as candidate, so UI filtering alone is not a hard rule until the council validates too.

---

## UI — research

**Files:** `src/ui/research/CurrentResearchPanel.cpp`, `include/ui/research/CurrentResearchPanel.h`,
`src/ui/research/ResearchView.cpp`, `include/ui/research/ResearchView.h`

**Assessment:** This is a thin, readable overlay: `ResearchView` owns one panel and closes on
Escape; `CurrentResearchPanel` draws label/target/progress from style config with no
business logic of its own. The dominant weakness is presentation correctness and null
handling — the panel shows config tech ids to the player and treats a missing
`ResearchManager` the same as “no research target,” which sibling panels already reject
loudly.

### [M] Do not render a null `ResearchManager` as “None”
`src/ui/research/CurrentResearchPanel.cpp:26` — `if (m_pResearch && m_pResearch->HasResearchTarget())`
collapses a null manager and a valid manager with no target into the same “None” branch, so a
wiring bug is invisible in the UI. Project guidelines prefer references and throwing on
unexpected null; `GrowthDisplay` already throws when its manager pointer is null
(`src/ui/base/GrowthDisplay.cpp:22-25`). The 2026-07-09 const-correctness fix left nullable
`const ResearchManager*` parameters; the silent null branch is still wrong. Direction: take
`const ResearchManager&` in both constructors (or throw if a pointer must remain) and leave
“None” only for `!HasResearchTarget()`.

### [M] Show the tech display name, not the config id
`src/ui/research/CurrentResearchPanel.cpp:28` — `DrawText(m_pResearch->GetResearchTarget(), …)`
paints `TechId` (e.g. `ethical_calculus`, `secrets_of_the_human_brain`). Config carries a
separate player-facing `name` (`TechConfig_t::name`, e.g. “Ethical Calculus”). That is wrong
output for a “Current Research Target” label, not a missing feature. Direction: render the
display name once `ResearchManager` exposes the current `TechConfig_t` (or its `name`); see
outside-slice note.

### [L] Convention and hygiene items
- `include/ui/research/ResearchView.h:18` — `m_pResearch` is stored and never read; the
  constructor forwards the parameter to `CurrentResearchPanel` and never uses the member.
- `include/ui/research/CurrentResearchPanel.h:16` — empty `HandleMouseClick` override duplicates
  the base default in `UIElement` and adds nothing.
- `src/ui/research/ResearchView.cpp:16-24` — unlike `SocialEngineeringView`, does not call
  `IGameView::HandleKey` before handling Escape, so future element key handlers on this view
  would never run.
- `include/ui/research/CurrentResearchPanel.h:5` — unused `#include <string>`.
- `src/ui/research/ResearchView.cpp:3` — unused `#include "graphics/Graphics.h"`.
- `include/ui/research/ResearchView.h:14` — `explicit` on a two-parameter constructor is a no-op.

**Observed outside slice:**
- `include/game/faction/ResearchManager.h:26` — `GetResearchTarget()` returns only `TechId`; there
  is no accessor for the current `TechConfig_t` / display `name` the research panel needs.
- `src/ui/ViewFactory.cpp:77-81` — `CreateResearchView` returns `nullptr` when there is no player
  faction; callers must tolerate a null view from the F2 shortcut factory.

---

## UI — satellite / orbital

**Files:** `src/ui/satellite/OrbitalAttackOutcomePopup.cpp`, `include/ui/satellite/OrbitalAttackOutcomePopup.h`, `src/ui/satellite/OrbitalAttackerPopup.cpp`, `include/ui/satellite/OrbitalAttackerPopup.h`, `src/ui/satellite/SatelliteButtonListPanel.cpp`, `include/ui/satellite/SatelliteButtonListPanel.h`, `src/ui/satellite/SatelliteLabeledButton.cpp`, `include/ui/satellite/SatelliteLabeledButton.h`, `src/ui/satellite/SatelliteSummaryPanel.cpp`, `include/ui/satellite/SatelliteSummaryPanel.h`, `src/ui/satellite/SatelliteView.cpp`, `include/ui/satellite/SatelliteView.h`

**Assessment:** The slice is a clear, small composition — `SatelliteView` owns mode/selection state, list panels and labeled buttons are thin, and the deferred post-attack rebuild (`m_bPendingAttackRefresh`) correctly avoids destroying the attacker popup mid-callback. The dominant weakness is missing modal input ownership: popups sit in the same element list as tabs/lists, so clicks outside the small popup rect still drive underlying controls and can tear down or stack dialogs.

### [H] Capture input while orbital popups are open
`src/ui/satellite/SatelliteView.cpp:157-178` / `src/ui/satellite/SatelliteView.cpp:199-301` — `OrbitalAttackerPopup` / `OrbitalAttackOutcomePopup` use `Style().layouts.popupSmall` (40%×40% rect). `SatelliteView` does not override `HandleMouse`, so `IGameView::HandleMouse` only delivers clicks to elements whose bounds contain the point. Clicks on tabs, Attack, or faction/target lists while a popup is open still run `SetMode_` / `OnAttackClicked_` / `SelectFaction_` / `SelectTarget_`, which call `Rebuild_()` and erase the open popup, or push a second attacker popup on top of the first. `OrbitalAttackerPopup.cpp:185-188` outside-click `Cancel_()` is unreachable for the same reason. Fix: while a popup is topmost, route mouse (and optionally keys) only to it — including outside clicks — and refuse selection/mode rebuilds until it closes.

### [M] Rebuild the entire view on every selection change
`src/ui/satellite/SatelliteView.cpp:120-138` / `src/ui/satellite/SatelliteView.cpp:199-301` — `SelectFaction_` and `SelectTarget_` always `Rebuild_()`, destroying and recreating tabs, Attack, and both list panels. `SatelliteButtonListPanel` claims mutual-exclusive selection but never updates `m_selectedId` or button selected state in place (`SatelliteButtonListPanel.cpp:45-50`); it only works because the parent tears the UI down. Prefer updating list selection locally (or a narrow refresh) so open chrome and any future transient state survive.

### [M] Attack actions fail silently when prerequisites are missing
`src/ui/satellite/SatelliteView.cpp:141-147` — `OnAttackClicked_` returns with no feedback if faction or target is unset, while the Attack control stays enabled. `src/ui/satellite/OrbitalAttackerPopup.cpp:39-47` — `Confirm_()` likewise no-ops when nothing is selected; the Attack button is always created enabled (`OrbitalAttackerPopup.cpp:85-89`). Players get a dead click instead of disabled chrome or a message (the empty-attacker path already uses `ShowOutcome_`).

### [M] Summary panel reallocates census data every frame
`src/ui/satellite/SatelliteSummaryPanel.cpp:34-64` — each `Render` rebuilds the orbital-type vector, faction pointer list, and a string-keyed `unordered_map` of census counts. Same class of per-frame UI rebuild that was previously removed from world/base displays. Cache until census/factions/registry inputs change, or compute in the view when entering Summary mode.

### [M] Null player faction is swallowed
`src/ui/satellite/SatelliteView.cpp:160-164` — `OpenAttackerPopup_` returns quietly when `GetPlayerFaction()` is null. Guidelines require throwing on unexpected null rather than silent no-ops; a missing player during this view is not a normal empty state (contrast the explicit outcome string used when attackers are empty at line 169).

### [L] Convention and hygiene items
- `include/ui/satellite/SatelliteView.h:3` — header pulls `BuildingConfigParser.h` only for `BuildingId_t`; prefer a lighter alias header.
- `src/ui/satellite/SatelliteView.cpp:258` — faction ids round-trip through `std::to_string` / `std::stoi` instead of a typed list API.
- `src/ui/satellite/OrbitalAttackOutcomePopup.cpp:43-48` / `OrbitalAttackerPopup.cpp:105-106` — popup padding/font ratios borrowed from `productionSelectorPopup`, coupling satellite chrome to an unrelated style block.
- `src/ui/satellite/SatelliteLabeledButton.h:19` — `SetSelected` is unused; selection changes rebuild buttons instead (`OrbitalAttackerPopup.cpp:59-63`).
- `src/ui/satellite/OrbitalAttackOutcomePopup.cpp:69-72` — null checks on `m_pOkButton` after the constructor always creates it.
- No UI/unit tests under `tests/` exercise satellite view selection, popup stacking, or attack confirm flow (game-layer ASAT is covered separately).

**Observed outside slice:**
- `include/ui/IGameView.h:55-62` — default `HandleMouse` Contains-gates all elements, so outside-click dismiss in other popups (`ProductionSelectorPopup`, `PopTypeSelectorPopup`, etc.) is similarly unreachable without a per-view override.
- `docs/architecture/ui-system.md` — does not mention `SatelliteView` or the satellite panels despite the architecting rule to keep diagrams current.

---

## UI — settings

**Files:** `src/ui/settings/SettingsPanel.cpp`, `include/ui/settings/SettingsPanel.h`, `src/ui/settings/SettingsView.cpp`, `include/ui/settings/SettingsView.h`, `include/ui/settings/SettingDescriptor.h`

**Assessment:** This slice is small and mostly clear: `SettingsView` is a thin Escape-to-close overlay, and `SettingsPanel` drives rows from a static `SettingDescriptor_t` table with accessors into `GameSettings` rather than raw member pointers. The dominant weakness is that `SettingScope_t` and descriptor invariants are only half-enforced — click/render paths assume a correct table and treat `NewGameOnly` as “never editable,” which will silently break the first editable new-game bool.

### [M] Make `NewGameOnly` session-aware instead of permanently non-editable
`include/ui/settings/SettingDescriptor.h:10-15` documents scope as controlling editability “in the current session context,” but neither `SettingsView` nor `SettingsPanel` receives any new-game/in-progress flag. `HandleMouseClick` skips every `NewGameOnly` bool unconditionally (`SettingsPanel.cpp:185-188`), so a future editable new-game bool would never toggle. Pass a session flag into the panel (or view) and allow `NewGameOnly` edits only when that flag is set; until then, do not use `NewGameOnly` on `Bool` rows.

### [M] Ignore non-left clicks in `HandleMouseClick`
`SettingsPanel.cpp:176-198` toggles and `Save()`s on any mouse button that reaches the handler. Peer UI elements require `MouseButton_t::Left` before acting. Right/middle press currently flips preferences and writes `user_settings.json`. Gate the handler on left button before hit-testing rows.

### [M] Enforce descriptor kind/callback invariants before calling through
`SettingDescriptor_t` defaults callbacks to null (`SettingDescriptor.h:29-31`). `Render` and `HandleMouseClick` invoke `getBool` / `setBool` / `getValueText` with no checks (`SettingsPanel.cpp:160`, `165`, `195`). A mismatched table row is undefined behavior; project guidelines prefer throwing on unexpected null. After resolving `kind`, throw if the required callback(s) are null (and treat unknown kinds as errors rather than falling through to `getValueText`).

### [M] Stop treating every non-header/non-bool row as `ReadOnlyValue`
`SettingsPanel.cpp:157-170` branches `Bool` vs else; the else always concatenates `getValueText(...)`. Adding a new `SettingRowKind_t` without updating this switch will crash or mis-render. Switch on `kind` exhaustively (or `default:` throw) so new row types fail loudly at the panel, not at the function pointer.

### [L] Convention and hygiene items
- `include/ui/settings/SettingsView.h:18` / `SettingsView.cpp:11-15` — `m_rSettings` is only forwarded in the constructor; drop the member and pass the ctor parameter straight into `SettingsPanel` (same dead-store pattern as some other views, still noise here).
- `src/ui/settings/SettingsPanel.cpp:135` — `const auto& style` should be `rStyle` per reference naming.
- `src/ui/settings/SettingsPanel.cpp:17-20` — prefer `GameSettings::IsPauseAtEndOfTurn()` over reaching into `GetGameRules().pauseAtEndOfTurn`.
- `src/ui/settings/SettingsPanel.cpp:145-173` and `180-198` — duplicate row-count / `rowHeight` / `RowAt_` loops; extract one helper that yields `(descriptor, area)` to keep hit-testing aligned with paint.
- Implemented bool toggle + persist has no UI-level test coverage (only `GameSettings` unit tests exist).

**Observed outside slice:**
- `docs/architecture/ui-system.md` — Settings view/panel are absent from the UI architecture diagram and overview despite being a live `ViewFactory` product.

---

## UI — social engineering

**Files:** `src/ui/social-engineering/SocialEngineeringBottomPanel.cpp`, `include/ui/social-engineering/SocialEngineeringBottomPanel.h`, `src/ui/social-engineering/SocialEngineeringDisplay.cpp`, `include/ui/social-engineering/SocialEngineeringDisplay.h`, `src/ui/social-engineering/SocialEngineeringView.cpp`, `include/ui/social-engineering/SocialEngineeringView.h`

**Assessment:** Policy grid layout, hit-testing, and active-policy mutation are coherent and share `GetPolicyCellLayout`, which keeps render and click paths aligned. Prior invented faction-rating logic and the fixed 4-column truncate are gone. The dominant weaknesses are a half-finished faction-bonus formatter that always shows `"None"`, and a bottom-panel research readout that uses the wrong Faction API for a “turns until breakthrough” label.

### [H] Finish `FormatFactionBonuses` — faction bonus line always shows "None"
`src/ui/social-engineering/SocialEngineeringDisplay.cpp:110-141` — The loop resolves each non-zero social rating, looks up `FindSocialRatingLevelEffects`, then discards `pLevelEffects` without writing to `oss` or clearing `first`. `Render` always draws `"None"` (`:455`) even when scores are non-zero and `config/social_rating_effects.json` has StatModifier/RuleFlag payloads. This is a wired, player-visible stub that silently lies. Format each level’s effects (at least StatModifier and RuleFlag) into `oss`, and throw (do not `continue`) when a known rating id is missing from the registry.

### [M] Bottom panel uses full-tech duration instead of remaining breakthrough turns
`src/ui/social-engineering/SocialEngineeringBottomPanel.cpp:87` — Label + `FormatTurnCount` present a turns-until-breakthrough figure, but the call is `GetBreakthroughRate()`, which `ResearchManager` documents as full turns ignoring accumulated progress. After any research progress the number is too high. Use `GetTurnsUntilBreakthrough()` instead.

### [M] Nullable deps deferred to Render; click path swallows null
`SocialEngineeringDisplay` / `BottomPanel` / `View` constructors accept `Faction*` / registry pointers with no validation (`SocialEngineeringDisplay.cpp:252-262`, `SocialEngineeringBottomPanel.cpp:32-38`, `SocialEngineeringView.cpp:10-31`). `Render` throws on null; `HandleMouseClick` silently returns (`SocialEngineeringDisplay.cpp:465-467`). Prefer references (or throw in the constructor) so a bad `ViewFactory` wiring fails at push time, not mid-frame / mid-click.

### [M] Hardcoded category and rating axis tables drift from enums
`SocialEngineeringDisplay.cpp:28-46` — `k_Categories` and `k_AllRatings` manually list every `SocialCategory_t` / `SocialRatingId_t`. A new axis in the enum + config will not appear in scores, hit-test loops, or the (intended) faction-bonus line until these arrays are edited. Drive iteration from `magic_enum::enum_values` (keep the one explicit display map for `Future Society`).

### [L] Convention and hygiene items
- `include/ui/social-engineering/SocialEngineeringView.h:25-27` / `SocialEngineeringView.cpp:17-19` — `m_pFaction`, `m_pPolicyRegistry`, `m_pRatingRegistry` are only forwarded in the constructor; dead members after child creation (unlike `ResearchView`, which still uses its pointer).
- `SocialEngineeringDisplay.cpp:147,154,373` — locals `isActive` should be `bIsActive` per boolean naming.
- `SocialEngineeringDisplay.cpp:78,116` — locals `first` should be `bFirst`.
- No tests cover SE UI formatting or click→`SetActivePolicy` (implemented paths); the always-`"None"` bonus line would have been an obvious case.

**Observed outside slice:**
- `docs/architecture/ui-system.md` — `SocialEngineeringView` / display / bottom panel are absent from the UI diagram and ViewFactory list (architecting rule: keep diagrams current).
- `src/ui/ViewFactory.cpp:96-100` — passes `socialPolicyRegistry.get()` / `socialRatingRegistry.get()` without null checks, so a missing registry becomes a late Render throw inside this slice.

---

## UI — style

**Files:** `src/ui/style/UiStyle.cpp`, `include/ui/style/UiStyle.h`

**Assessment:** This slice is the config-backed theme extraction that prior review 4.5 deferred (“shared UI theme”). Load fails loudly on missing files/keys, commits into the global only after a full parse, and `Get` throws if used too early — good failure posture. The dominant weakness is structural: a process-global mega-struct plus ~40 hand-rolled section parsers that must be edited for every new UI surface, with duplicated type pairs and world-domain values mixed into presentation config.

### [H] Stop growing a process-global god-object style registry
`include/ui/style/UiStyle.h:574`–`621` and `src/ui/style/UiStyle.cpp:15`–`16`, `690`–`749` — `UiStyle` is both a ~40-member typed bag and a file-scope singleton (`g_style` / `g_loaded`) accessed via `Style()` (`include/ui/style/UiStyle.h:623`). Every new panel requires a new nested struct, a `UiStyle` member, a `Parse*Style_` clone, and another `root.at(...)` line in `Load`. That violates open/closed growth and bypasses the project’s owned definition-data pattern (`GameDataContext`): UI code cannot take a `const UiStyle&` at construction, and tests cannot inject an alternate theme without mutating process state. Split into per-feature style types (or section parsers) loaded into an owned object and passed down from startup/factory; keep `Style()` only as a temporary bridge if needed.

### [M] Collapse duplicate identical style type pairs
`include/ui/style/UiStyle.h:294`–`318` (`GrowthDisplayStyle` / `ProductionDisplayStyle`) and `363`–`389` (`PopTypeSelectorPopupStyle` / `ProductionSelectorPopupStyle`), with matching parse twins at `src/ui/style/UiStyle.cpp:374`–`402` and `453`–`483` — the struct layouts, JSON keys, and current `config/ui/style.json` values are identical, yet each pair is maintained twice. A one-sided tweak silently desyncs two screens. Share one type (and one parser) per pair, or alias the second name to the first.

### [M] Do not store world elevation range in UI tile style
`include/ui/style/UiStyle.h:43`–`44`, `src/ui/style/UiStyle.cpp:84`–`85` — `minElevationMeters` / `maxElevationMeters` duplicate `min_elevation` / `max_elevation` from `config/worldGen/presets.json` (same ±4000 today). Tile fill remapping reads the UI copy (`TileRenderer` via `Style().tileRenderer`), so a world-gen or mod elevation change that does not update `style.json` silently wrong-colors the map. Keep only visual knobs in style; take the elevation domain from map/world config (or a single shared constants source).

### [L] Convention and hygiene items
- `include/ui/style/UiStyle.h:11`–`572` — nested style bags are config/POD structs but omit the required `_t` suffix (`LayoutsStyle`, `TileRendererStyle`, …); the aggregate should be `UiStyle_t` (or similar), not a class with all-public data (`574`–`621`).
- `include/ui/style/UiStyle.h:580`, `src/ui/style/UiStyle.cpp:762`–`765` — `IsLoaded()` is unused outside this TU; dead API next to `Get()`’s throw-on-unloaded path.
- `src/ui/style/UiStyle.cpp:21` — `ParseColor_` accepts arrays longer than 4 and ignores the extras (layouts require exact length 4 at `:36`); tighten to size 3 or 4 for consistency.

**Observed outside slice:**
- `docs/architecture/ui-system.md` — UiStyle / `config/ui/style.json` are absent from the UI architecture diagram despite being a cross-cutting UI dependency.
- `src/ui/commlinks/CouncilProposalsPopup.cpp`, `src/ui/world/ProbeActionPopup.cpp` — reuse `Style().productionSelectorPopup` instead of dedicated sections (call-site smell; may need new style keys here when fixed).

---

## UI — unit designer

**Files:** `src/ui/unit-designer/ComponentSelectorPopup.cpp`, `include/ui/unit-designer/ComponentSelectorPopup.h`, `src/ui/unit-designer/ComponentSlotDisplay.cpp`, `include/ui/unit-designer/ComponentSlotDisplay.h`, `src/ui/unit-designer/DesignListPanel.cpp`, `include/ui/unit-designer/DesignListPanel.h`, `src/ui/unit-designer/DesignStatsDisplay.cpp`, `include/ui/unit-designer/DesignStatsDisplay.h`, `src/ui/unit-designer/SlotColumnPanel.cpp`, `include/ui/unit-designer/SlotColumnPanel.h`, `src/ui/unit-designer/UnitDesignerView.cpp`, `include/ui/unit-designer/UnitDesignerView.h`, `src/ui/unit-designer/UnitStatusPanel.cpp`, `include/ui/unit-designer/UnitStatusPanel.h`, `include/ui/unit-designer/UnitDesignerState.h`

**Assessment:** The view is well factored into slot columns, stats, design list, and status panels, with layout driven by `UnitSlotRegistry` and UI style config. Dominant weaknesses are a dead parallel slot widget (`ComponentSlotDisplay`), missing tech gating despite populated `required_tech` data, and draft/selection state that diverges after the player edits a loaded design. Popup and list overflow behavior will also trap maintainers as catalogs grow.

### [H] Ignore component `requiredTech` when listing and saving
`src/ui/unit-designer/UnitDesignerView.cpp:137-144` — `ShowComponentSelector_` pushes every registry entry whose `type` matches; nothing consults `UnitComponentConfig_t::requiredTech` (or slot `requiredTech`). Production config already gates many components (e.g. `orbital_spaceflight`, `planetary_networks`), and other systems treat empty/`HasDiscoveredTech` as the availability contract. Players can assemble and `HandleSaveDesign_` designs that should be locked. Filter the available list (and optionally hide locked slots) against faction research; thread a research/availability dependency into the view constructor.

### [H] Retire or reuse dead `ComponentSlotDisplay`
`src/ui/unit-designer/ComponentSlotDisplay.cpp:1-64`, `src/ui/unit-designer/SlotColumnPanel.cpp:116-150` — `ComponentSlotDisplay` is compiled and styled but never constructed; `UnitDesignerView` builds `SlotColumnPanel` instead, which reimplements the same fill/border/label/name paint and click callback. Architecture docs still describe per-slot `ComponentSlotDisplay` children. Two UIs for one job will drift. Either compose `SlotColumnPanel` from `ComponentSlotDisplay` children or delete the unused class and its style block, and update the diagram.

### [M] Component selector is not modal; popups can stack
`src/ui/unit-designer/UnitDesignerView.cpp:146-150`, `include/ui/IGameView.h:55-61` — each slot click `push_back`s a `ComponentSelectorPopup` without dismissing an existing one. `IGameView::HandleMouse` only delivers clicks to an element that `Contains` the cursor, so clicks outside the small popup reach slots/stats underneath and open another popup. Close any open selector before opening a new one, or make the popup consume outside clicks (dismiss or no-op).

### [M] Selected design and draft state desync after edits
`src/ui/unit-designer/UnitDesignerView.cpp:104-116`, `src/ui/unit-designer/UnitStatusPanel.cpp:48-66` — `OnDesignSelected_` sets `m_pSelectedDesign` and copies components into `m_state`. Later slot changes update only `m_state`; selection is left pointing at the saved design. `UnitStatusPanel` then shows that design’s name/active count while slots and `DesignStatsDisplay` show the edited draft, and `DesignListPanel` keeps its highlight. Clear selection (and list highlight) when the draft diverges, or explicitly enter an “editing design X” mode.

### [M] Design list silently truncates overflow designs
`src/ui/unit-designer/DesignListPanel.cpp:61-88` — boxes are laid out horizontally and `Render` `break`s when the next box would exceed panel width; there is no scroll or overflow cue. Extra designs are invisible and unreachable (hit-testing never receives clicks outside the panel). Add horizontal scroll/paging, or wrap, matching `SlotColumnPanel`’s scroll pattern.

### [M] Unknown slot `column` values fall through to left
`src/ui/unit-designer/UnitDesignerView.cpp:65-72` — only `"right"` is recognized; any other string (including typos) is treated as left. Config documents `"left"` or `"right"`. Throw (or validate at slot-registry load) on unexpected `column` instead of silently misplacing slots.

### [L] Convention and hygiene items
- `include/ui/unit-designer/DesignStatsDisplay.h:16-17` / `DesignListPanel.h:16` — prefer references over nullable pointers for always-valid deps (`UnitDesignerState_t`, slots, `Military`); guidelines require throw-on-null, not unchecked deref (`DesignStatsDisplay.cpp:58`).
- `include/ui/unit-designer/DesignListPanel.h:25` — `SetSelectedDesign` is public but never called; selection only updates inside `HandleMouseClick`.
- `include/ui/unit-designer/UnitDesignerView.h:38` — uses `std::function` / `std::string` without `#include <functional>` / `<string>` (transitive today).
- `src/ui/unit-designer/DesignStatsDisplay.cpp:70` — builds a temporary `UnitDesign` every `Render` frame; cache on state change or share with the save path.
- `src/ui/unit-designer/ComponentSelectorPopup.cpp:31-35` — entry rects can extend past popup height with no scroll/clip; same growth hazard as the design list.

**Observed outside slice:**
- `docs/architecture/unit-designer-system.md` — still describes `ComponentSlotDisplay` ownership and a `nullptr` UnitManager TODO; code uses `SlotColumnPanel` and `ViewFactory` already passes `GetUnitManager()` (`src/ui/ViewFactory.cpp:118`).
- Tech-availability filtering will also need a research/faction dependency wired at `ViewFactory::CreateUnitDesignerView` once the view filters `requiredTech`.

---

## UI — world map and combat presentation

**Files:** `src/ui/world/CameraInputController.cpp`, `include/ui/world/CameraInputController.h`, `src/ui/world/CombatPresentation.cpp`, `include/ui/world/CombatPresentation.h`, `src/ui/world/CombatView.cpp`, `include/ui/world/CombatView.h`, `src/ui/world/MapViewport.cpp`, `include/ui/world/MapViewport.h`, `src/ui/world/MinimapDisplay.cpp`, `include/ui/world/MinimapDisplay.h`, `src/ui/world/UnitMarkerRenderer.cpp`, `include/ui/world/UnitMarkerRenderer.h`, `src/ui/world/UnitOrderInputController.cpp`, `include/ui/world/UnitOrderInputController.h`, `src/ui/world/WorldDisplay.cpp`, `include/ui/world/WorldDisplay.h`, `src/ui/world/WorldView.cpp`, `include/ui/world/WorldView.h`

**Assessment:** `MapViewport` cleanly owns cylindrical wrap math, and the split between `WorldDisplay` / `UnitMarkerRenderer` / input controllers is mostly coherent. Dominant weaknesses are modal lifetime and turn-gating in `WorldView` (in-view popups are not real overlays), plus combat playback that reads live map state after `Resolve` has already mutated positions. `CameraInputController` and `MinimapDisplay` are in good shape.

### [H] Gate turns and map input while WorldView modal popups are open
`src/ui/world/WorldView.cpp:320-335`, `src/ui/world/WorldView.cpp:541-571`, `src/ui/world/WorldView.cpp:387-390`, `src/ui/world/WorldView.cpp:273-278`, `src/ui/world/WorldView.cpp:404-418` — `SupplyCrawlPopup` / `ProbeActionPopup` are pushed onto `m_elements` and capture raw `Unit*`, but unlike `BaseView` they are not overlay views. `Enter`, the End Turn button, and auto-advance in `Update_` can still call `m_onProcessTurn()`; `OnUnitDestroyed` clears `m_pSelectedUnit` but the lambda still compares/uses a dangling `pUnit`/`pProbe`. Chrome hit-testing only forwards presses to elements that `Contains` the cursor, so the popups' click-outside dismiss path never runs and map selection continues underneath. This reopens the class of defect marked fixed in review 1.8 for overlay popups — treat these as modal (block turn + route all input to the popup, or push a real overlay).

### [H] Do not open BaseView when a visible unit was selected on the tile
`src/ui/world/WorldView.cpp:494-503` — after `SelectUnitAtTile_`, the handler always calls `m_onOpenBase` when `FindBaseAt` hits. Architecture (`docs/architecture/ui-system.md`) requires base-open only when the tile has no (visible) units. As written, every garrison click steals focus into BaseView, and `FindBaseAt` can return any faction's base. Open the base only when selection cleared to no unit (or via an explicit base UI affordance).

### [M] Prefer fight-tile hit placement for the whole combat playback
`src/ui/world/CombatPresentation.cpp:103-132` — `Render` prefers the live `UnitMarkerRenderer` cache and only ghosts onto `m_pAttackerTile` / `m_pDefenderTile` when `WasDestroyed_`. `CombatResult_t` documents that Resolve already applied retreat/`DestroyUnit` before playback; a disengaged survivor is drawn at `pRetreatTile`, so mid-replay hit flashes appear on the retreat tile, not the fight tile. While `IsActive()`, place overlays from the recorded combatant tiles (cache only as a same-tile refinement), matching the “replay history” contract.

### [M] Do not run ProcessTurn from inside Render
`src/ui/world/WorldView.cpp:125-132`, `src/ui/world/WorldView.cpp:273-278` — with pause-at-end off, `Update_()` (called from `Render`) invokes `m_onProcessTurn()` when the last unit stops needing orders. That mutates the full turn pipeline mid-frame after the world has already been drawn and before overlays render — reentrancy and half-updated UI. Queue the auto-end-turn for the input/update phase (or a post-render callback), not the draw path.

### [M] Resolve defender display name with the same unit Resolve attacked
`src/ui/world/WorldView.cpp:574-584`, `src/ui/world/WorldView.cpp:511-512` — `FindUnitNameOnTile_` returns the first non-null unit on the tile. Stacked units make the CombatView defender label wrong relative to `TryAttack` / `FindAttackableHostileOnTile`. Capture the defender name from the unit actually fought (or the attackable hostile lookup) before Resolve.

### [M] Stop hardcoding improvement IDs in the map renderer
`src/ui/world/WorldDisplay.cpp:154`, `src/ui/world/WorldDisplay.cpp:186` — Sensor/Monolith markers key on string literals `"Sensor"` / `"Monolith"`. Markers silently vanish if config ids change; drive marker kinds from improvement config (or a style/config map of id → marker), not compile-time strings.

### [L] Convention and hygiene items
- `src/ui/world/WorldDisplay.cpp:27-41` / `src/ui/world/MinimapDisplay.cpp:23-37` — duplicated `PlayerFogMaps_t` / `PlayerFog_` helpers with divergent member names (`explored` vs `pExplored`).
- `src/ui/world/CameraInputController.cpp:113-126` — uses `s.relativeMin` as the “no scroll” sentinel; works only while `relative_min` stays `0.0` in style JSON — use an explicit zero / optional direction instead.
- `src/ui/world/UnitMarkerRenderer.cpp:48-51` — unexpected null `Unit*` in the tile stack is skipped; project rules prefer throw on unexpected null.
- `src/ui/world/UnitOrderInputController.h:29-30` — nullable `GameState*` / `GameDataContext*` default to silent no-op for attack/probe; call sites always pass them — prefer references (or assert) once the deferred path is gone.
- `include/ui/world/UnitMarkerRenderer.h` / `WorldDisplay` — almost no automated coverage for wrap hit-testing, hold-to-move, or combat playback timing despite substantial branching.

**Observed outside slice:**
- `src/ui/UIManager.cpp:86-105` — overlay `ShouldClose` is checked before `CombatView::Render` runs `Update`, so close lags one frame (acceptable, but the only place playback advances).
- `docs/architecture/ui-system.md` — still describes right-click hold for move orders; implementation is left-click hold in `UnitOrderInputController`.

---

## UI — world panels and order input

**Files:** `src/ui/world/CommlinksButton.cpp`, `include/ui/world/CommlinksButton.h`, `src/ui/world/EndTurnButton.cpp`, `include/ui/world/EndTurnButton.h`, `src/ui/world/InfoPanelElement.cpp`, `include/ui/world/InfoPanelElement.h`, `src/ui/world/LocationPanel.cpp`, `include/ui/world/LocationPanel.h`, `src/ui/world/ProbeActionPopup.cpp`, `include/ui/world/ProbeActionPopup.h`, `src/ui/world/SelectedUnitPanel.cpp`, `include/ui/world/SelectedUnitPanel.h`, `src/ui/world/SupplyCrawlPopup.cpp`, `include/ui/world/SupplyCrawlPopup.h`, `src/ui/world/TerraformInputController.cpp`, `include/ui/world/TerraformInputController.h`, `src/ui/world/UnitStackPanel.cpp`, `include/ui/world/UnitStackPanel.h`

**Assessment:** Dashboard panels (`SelectedUnitPanel`, `LocationPanel`, `InfoPanelElement`, `UnitStackPanel`) and the two chrome buttons are small, style-driven, and easy to follow. The weak spots are order-entry surfaces: terraform hotkeys hardcode every improvement id in C++, and `ProbeActionPopup` / `SupplyCrawlPopup` are forked copies of `ProductionSelectorPopup` with the same unreachable outside-dismiss and soft callback guards. Nothing in this slice is large; the risk is silent key/config drift and modal UX that looks finished but is not.

### [H] Load terraform key→improvement bindings from config
`include/ui/world/TerraformInputController.h:31-54` hardcodes twenty-two `Key_t` → improvement id strings (`"Road"`, `"Farm"`, `"SolarCollector"`, …) that must stay byte-identical to `config/improvements.json`. Renames, new Former projects, or modded hotkeys require a C++ edit; a mismatch only fails later inside `TryStartTerraform` with no construction-time check. Drive the map from config (or improvement metadata) and validate ids against the registry when the controller is built.

### [H] Stop cloning ProductionSelector for probe and supply popups
`src/ui/world/ProbeActionPopup.cpp` and `src/ui/world/SupplyCrawlPopup.cpp` duplicate the ProductionSelector layout/render/Escape/click path (same `Style().productionSelectorPopup`, same `CacheEntryRects_`, same dismiss shape), already called out for the base selectors in part 29. They will keep drifting (labels, empty-callback policy, overflow) independently. Collapse onto one typed list-selector helper so dismiss, hit-testing, and clipping have a single owner.

### [M] Outside-click dismiss in probe/supply popups is unreachable
`src/ui/world/ProbeActionPopup.cpp:84-88` and `src/ui/world/SupplyCrawlPopup.cpp:82-86` set `m_bShouldClose` when the click is outside `m_layout`, but `WorldView::HandleMouse` (and `IGameView::HandleMouse`) only invoke `HandleMouseClick` after `Contains` on that layout. The branch never runs; Escape is the only in-popup dismiss. Remove the dead branch until modal routing exists, or give the popup a fullscreen hit target with inset chrome.

### [M] UnitStackPanel silently drops units that do not fit
`src/ui/world/UnitStackPanel.cpp:49-57` stops laying out when `x + slotWidth > right`, with no scroll, wrap, or overflow cue. Stacks wider than the panel become partially invisible and unselectable even though `SetUnits` received them. Bound visibly and offer scroll/paging (or signal overflow) so tile/base stacks remain fully reachable.

### [M] Soft-skip empty callbacks instead of requiring a valid handler
`CommlinksButton.cpp:29-32`, `EndTurnButton.cpp:31-34`, `ProbeActionPopup.cpp:94-97`, `SupplyCrawlPopup.cpp:92-95`, and `UnitStackPanel.cpp:101-104` no-op when the `std::function` is empty. Probe/supply also leave the popup open on an empty handler after a row hit. Against “throw on unexpected null / construct only valid objects”: require a non-empty callback in the constructor (and dismiss the popup after a resolved row).

### [M] Hardcode supply-crawl resource choices in the popup
`src/ui/world/SupplyCrawlPopup.cpp:16-20` fixes Nutrients/Minerals/Energy labels and `StatId_t` values in C++. Crawlable stats belong in config (or a shared rules table) so a mod cannot extend the picker without editing this file. Build the entry list from data the order rules already trust.

### [L] Convention and hygiene items
- `include/ui/world/TerraformInputController.h:31-54` — `m_bindings` is a per-instance `const unordered_map` data member; prefer `static constexpr` / file-scope table once bindings leave hardcoding.
- `src/ui/world/ProbeActionPopup.cpp:45` / `SupplyCrawlPopup.cpp:48` — reuse `Style().productionSelectorPopup` with no dedicated style keys; production chrome tweaks restyle these menus.
- `src/ui/world/UnitStackPanel.cpp:51-54` — null `pUnit` uses `break` (truncates the rest of the row) instead of `continue`; callers currently filter nulls, so this is a latent trap.
- `include/ui/world/ProbeActionPopup.h:24` / `SupplyCrawlPopup.h:24` — redundant `~…() override = default`.
- `include/ui/world/InfoPanelElement.h:23` — `InfoLine` default color calls `Style()` in a default member initializer (fine only after `UiStyle::Load`).
- No tests under `tests/` cover terraform key mapping, probe/supply hit-testing/Escape, or unit-stack overflow/click selection.

**Observed outside slice:**
- `src/ui/world/WorldView.cpp:404-418` — while `SupplyCrawlPopup` / `ProbeActionPopup` are open, clicks outside their rects fall through to map/order handling (move preview, tile select) instead of dismissing or being swallowed; modal capture belongs in view mouse routing.
- `src/ui/world/WorldView.cpp:301-360` vs terraform `O`/`B`/`L` — order-controller keys overlap Former bindings; L has an intentional attach fall-through, but O/B still steal Forest/Bunker when the unit also has SupplyCrawl/FoundBase.
- `docs/architecture/ui-system.md` — still omits these world dashboard panels, terraform controller, and probe/supply popups.
