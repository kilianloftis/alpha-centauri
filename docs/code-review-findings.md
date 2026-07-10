# Project Code Review — Architecture, Maintainability, Scalability

**Date:** July 2026
**Scope:** Full project (`include/`, `src/`, `tests/`, `config/`, `docs/`, build system). All source files were read; key claims were re-verified against the code before recording.
**Ground rules for this review:** Missing features are *not* findings — this is a game under construction. Findings are limited to structural problems: patterns that will not scale, designs that fight the stated guidelines, state that can desync, and decisions made silently that should have been explicit. Existing patterns were not given the benefit of the doubt just because they are applied consistently. No fixes are proposed here; this document only identifies issues.

Severity tags: **[H]** structural problem that compounds as the codebase grows · **[M]** localized design flaw or trap · **[L]** hygiene/consistency issue.

Related prior review: `docs/architecture/effects-system-review.md` (effects subsystem only; this document covers the whole project and does not repeat its items).

---

## 1. Systemic architecture

### 1.1 [H] No strategy for derived state: everything is recomputed from scratch on every read

> **Status (2026-07-09): the invalidation seam exists and the dominant recompute paths are
> memoized.** Deliberately deferred sub-items listed at the end of this note.
>
> Mechanism — pull-based revision counters (`lib/Revision.h`) plus a two-level cache:
> - Every pool contributor owns a `Revision` and bumps it in its own mutators (a *local*
>   invariant — no cross-object wiring, avoiding 1.6's manual-signal fragility):
>   `BuildingManager` (add/destroy), `PopContainer` (add/remove/convert, exposed via
>   `PopulationManager`), `UnitManager` (create/destroy), `SocialEngineeringManager`
>   (policy set), and `Faction`'s base-list (`AddBase`).
> - `IEffectsProvider` now returns `const FactionEffects_t&` plus a monotonic
>   `GetEffectsVersion()`. Pool assembly + memoization live in a dedicated component,
>   `FactionEffectsPool` (`game/faction/FactionEffectsPool.{h,cpp}`), which validates a
>   stamp of contributor revisions (a handful of integer compares) and rebuilds only on
>   mismatch; `Faction` implements `IEffectsProvider` by delegating to it. UI frames and
>   unit stat reads no longer rebuild the pool.
> - `BaseManager::BuildBaseEffects_()` (no-arg) memoizes the filtered+expanded base list
>   keyed on the provider's pool version, so the five production getters,
>   `GetNutrientsRequired`, and `GetWorkedTileYield` (20×/frame) stop re-filtering. The
>   explicit-pool overload used by the per-turn stages stays uncached by design.
> - The review's named stale-cache example is fixed: `ResearchManager` revalidates
>   `m_pointsNeededForCurrentTech` against the provider version, so a `TechCost` effect
>   appearing mid-research is reflected on the next read (regression-tested).
> - Guarded by `tests/effects/EffectsCacheTests.cpp` (version stability, per-contributor
>   invalidation incl. content checks, mid-research cost change) — plus the existing
>   routing/integration tests, which assert post-mutation values and therefore fail loudly
>   on any missed bump. Enabling fix: `UnitManager::GetUnits()` (last leaked owning
>   container) replaced by a `Units()` reference range.
>
> Done 2026-07-09 (filter-chain copies): the "cheap now that the pool is cached" note above
> undersold this — investigation found the actual chain worse than assumed: each of
> `BaseManager`'s 5 stat getters independently resolves every worked tile via
> `TileEffectsContext::ResolveTileYield`, which called `FilterByStatId` 3× per tile, i.e.
> `15×(W+1)` copying filter calls (W = worked tiles) every time all 5 stats are read — every
> frame the base screen is open. `FilterByStatId`, `FilterByStatIdInContext`,
> `FilterFlatByStatId`, `FilterByScope` (`lib/effects/ActiveEffect.h`) now return a lazy
> `std::views::filter` range instead of an allocated copy — non-template functions with a
> deduced `auto` return type must be `inline` and defined where used, so their bodies moved
> from the `.cpp` into the header, matching the `DerefView` pattern already used for
> `Faction::Bases()` etc. `ResolveStatModifiers` became a function template over
> `std::ranges::input_range` for the same reason, absorbing ~24 `Filter*(...) →
> ResolveStatModifiers(...)` call sites with no call-site changes. `FilterForBase` was
> deliberately left eager — its result is cached and mutated by `BaseManager`, so laziness
> would relocate the allocation, not remove it. Three call sites (`Pop.cpp` ×2,
> `CollectFromPops`) bind a filtered result to a name reused across *later* statements (a
> lambda invoked repeatedly, a following loop); since C++ destroys a temporary at the end of
> its full expression, these needed forced eager materialization to avoid a dangling view —
> found by reading each site directly, not assumed. Two sites (`Unit.cpp`, `GameState.cpp`)
> were safe to go fully lazy and had their now-unnecessary copy removed. Full test suite
> (541 assertions) passes unchanged — pure copy elimination, no predicate/behavior change.
>
> Also settled while investigating (no code change):
> - `GameState::CollectWorldEffects`'s O(F²) shape: **not a real problem** — 2 calls per
>   faction per *turn* (not per frame), each already filtering a cached pool. Closing with
>   no action; the doc's earlier "deferred" framing overstated this.
> - `ResourceManager::ComputeWorked_`'s 5× redundant call per stat-read cycle: **confirmed
>   blocked on finding 2.1**, not merely "entangled" — there is currently zero revision
>   tracking anywhere on worker-tile assignment (`Pop::SetTile`/`Tile::m_bWorked` are bare
>   mutations) to key a cache on. Building that is 2.1's job (worker state ownership),
>   not a tag-along here. *(Unblocked 2026-07-09: 2.1's fix added `WorkedTileIndex` with a
>   `GetRevision()` bumped on every claim/release; the memoization itself is still to do.)*
>
> Still deferred: `TileEffectsContext::CollectAreaEffects`'s (2r+1)² per-tile neighborhood
> scan — confirmed hotter than originally described (it runs inside the sort comparator in
> `WorkerAssignmentManager::PrioritizeAvailableTiles_`, so every auto-assign re-scans
> neighborhoods per tile per comparison, not just the UI display path). Needs an actual
> incremental spatial index (what invalidates on a unit move vs. an improvement change) —
> a different mechanism from the revision-counter seam used everywhere else in this finding,
> not a variant of it. Deliberately not attempted as a tag-on; would need its own scoping.

The single biggest scalability issue in the codebase, and it is a *pattern*, not an isolated slip:

- `Faction::GetActiveEffects()` (`src/game/Faction.cpp:430`) rebuilds the entire faction effect pool on every call: walks all bases → all buildings → expands grant chains (`ExpandGrantBuildingEffects`) → all pops → all units, allocating vectors of `ActiveEffect_t` (each holding a `std::string sourceId`) at every stage.
- `BaseManager::BuildBaseEffects_()` (`src/game/faction/base/BaseManager.cpp:334`) calls `GetActiveEffects()` — and it is invoked by **every single stat getter**: `GetNutrientProduction`, `GetMineralProduction`, `GetEconProduction`, `GetLabsProduction`, `GetPsychProduction`, `GetNutrientsRequired`, `GetWorkedTileYield` (per tile!), `GetEffectiveSocialRating` (per rating!).
- `ResourceManager` compounds it: the five production getters each independently call `ComputeWorked_` (`src/game/faction/base/resources/ResourceManager.cpp`), which re-resolves every worked tile. Reading all five stats = five full worked-tile passes plus five pool rebuilds.
- `TileEffectsContext::CollectAreaEffects` (`src/lib/effects/TileEffectsContext.cpp`) scans a `(2·maxRadius+1)²` neighborhood per tile query, where `m_maxRadius` is the **global maximum** radius across all improvement and unit-component configs. One modded improvement with radius 5 makes every tile yield query in the game scan 121 tiles, each allocating `std::vector<std::string>` from `Tile::GetTerrainFeatureIds()` and doing registry lookups.
- `Unit::ResolveStat_` (`src/game/units/Unit.cpp`) rebuilds the faction pool per stat read. A combat preview reading attack/defense/HP of two units = 6 full pool rebuilds.
- The UI drives these paths **per frame** (60 FPS cap set in `SFMLGraphics`): `GrowthDisplay::Render` makes 2 pool rebuilds/frame (`src/ui/base/GrowthDisplay.cpp:56,60`), `BaseWorkableAreaDisplay::RenderTile_` does it for each of 20 tiles/frame (`src/ui/base/BaseWorkableAreaDisplay.cpp:93`), `SocialEngineeringDisplay` does it 10× per frame — once per rating axis (`src/ui/social-engineering/SocialEngineeringDisplay.cpp:188`). Order of magnitude: several hundred to >1000 full faction-pool rebuilds per second while the base screen is open, with two factions and one base each.

The problem is not today's frame time — it is that **no architectural seam exists to ever fix this**. There is no snapshot object, no dirty flag, no "effects changed" signal, no per-turn cache. Every consumer calls the recompute entry points directly, so introducing caching later means touching every call site and re-auditing every invariant. The one existing cache (`ResearchManager::m_pointsNeededForCurrentTech`) demonstrates the missing invalidation story: it is computed when the target is set and goes stale if a `TechCost`-modifying effect appears mid-research.

Related: every filter in the effects pipeline (`FilterByStatId`, `FilterForBase`, `FilterByScope`, …) returns a **copied** `std::vector<ActiveEffect_t>`; a typical resolution chains 2–3 filters, copying `std::string` per element per stage. `GameState::CollectWorldEffects` copies per faction per stage per turn (O(F²) per turn). *(Addressed 2026-07-09, see the status block above: all filters but `FilterForBase` are now lazy views; `CollectWorldEffects`'s O(F²) shape was investigated and found to not be a real problem at turn cadence.)*

### 1.2 [H] The `lib/` layer depends on `game/` — the layering is fictional

> **Status (2026-07-10): fixed.** `grep -rn '#include "game/' include/lib src/lib` is now
> empty.

`lib/` presents itself as generic infrastructure, but:

- `include/lib/GameEvent.h:3` includes `game/research/TechConfigParser.h` (to get `TechId`) — the "stable ABI mod-facing" event header depends on a config parser. *(Addressed 2026-07-10: dropped the include; `GameEvent.h` now declares `using TechId = std::string;` locally, mirroring the `FactionId` alias it already declared locally in the same file — an already-precedented duplication, see finding 9.)*
- `include/lib/EventBridge.h:4` includes `game/faction/base/BaseManager.h`. *(Addressed 2026-07-10: `EventBridge` moved to `include/game/EventBridge.h` / `src/game/EventBridge.cpp` — it exists solely to translate `BaseManager`'s signals into bus events, so it belongs with the thing it wires, not with the generic bus.)*
- `include/lib/effects/ActiveEffect.h` and `TileEffectsContext.h` include `game/faction/base/BaseTypes.h`; their `.cpp` files include 15+ game headers (`Faction`, `BaseManager`, `Tile`, `PopContainer`, `SocialEngineeringManager`, …). *(Addressed 2026-07-10: the whole `lib/effects` directory moved to `include/game/effects` / `src/game/effects` — per this finding's own conclusion, it's game logic, not reusable engine infra, so the honest fix is to stop pretending otherwise.)*

`lib/effects` is game logic in a `lib` costume. Consequences: `lib` cannot be reused or tested standalone, the include graph has no enforceable direction, and the "mod-facing stable ABI" claim of `EventBus`/`GameEvent` is hollow while its types are transitively defined by internal game headers. Either the boundary means something or it should not exist; right now it misleads. *(Also found and fixed while closing this out: `src/lib/config/ConfigFields.cpp` pulled in `game/GameCategory.h` for one JSON-parsing overload — moved to `game/GameCategory.h`/`.cpp` as `ParseGameCategoryField`, renamed to avoid an overload ambiguity with the existing `ParseGameCategory(const std::string&)` when called with a string literal. `lib/` now has zero `game/` includes anywhere, header or `.cpp`.)*

### 1.3 [H] `GameDataContext` is a service locator whose own invariant is already broken

> **Status (2026-07-10): fixed**, scoped per direction: composition roots/factories
> (`Engine`, `Faction::CreateBase`, the `tests/GameFixtures.h` test harness) still pass
> `GameDataContext` around wholesale; the leaf classes it feeds (`BaseManager`,
> `PopulationManager`, `BuildingManager`) now declare narrow, named dependencies instead of
> taking the whole struct.

`include/game/GameDataContext.h` documents itself as "immutable definition data loaded once at startup; never serialised — always reconstructible from config files." In reality it holds:

- `LuaRuntime` — a mutable interpreter whose global state is shared and mutated by every formula evaluation (see 3.6).
- Seven calculators — *services*, not data.
- `SecretProjectAvailabilityCalculator` — which holds a pointer **into `GameState`'s live faction vector** (`src/game/Engine.cpp:193`), i.e., the "immutable definition data" object references mutable save-game state. It must be constructed mid-initialization after the world exists, and would dangle across any future load-game/new-game that rebuilds `GameState`. *(Addressed 2026-07-10: moved to `GameState` as an owned-by-value member, constructed with `*this` in `GameState`'s own constructor — the same self-referential idiom already used elsewhere. It's now an owned member of the object it queries instead of an external object holding a reference into it, so the "constructed after GameState exists" ordering problem and the future-rebuild dangling risk are both closed by construction. `Faction::CreateBase` threads it through the same way `TileEffectsContext` already was, via a new `const SecretProjectAvailabilityCalculator&` parameter sourced from `GameState::GetSecretProjectAvailability()`.)*

The struct is also a flat grab-bag: 19 `unique_ptr` members with no grouping, passed around wholesale (`BaseManager`, `PopulationManager` take the whole context and pluck what they need — hidden dependencies, DIP in letter but not spirit). *(Addressed 2026-07-10, scoped: `BaseManager`, `PopulationManager`, and `BuildingManager` constructors now take the specific registries/calculators they use as named parameters instead of `const GameDataContext&`; `Faction::CreateBase` (a factory method) is the unpacking point, pulling ~8 fields out of `rDataContext` to pass individually into `BaseManager`'s constructor. `Engine` and `GameFixtures.h` — the composition roots/factories — still hold and pass `GameDataContext` wholesale, per the agreed scope boundary. Separately, the struct's members are now grouped under `// --- Registries and config structs ---` / `// --- Calculators / services ---` section comments, and its class comment no longer claims blanket immutability — it now states plainly that `LuaRuntime` carries mutable interpreter state (see 3.6) and that anything reading live save-game state, like `SecretProjectAvailabilityCalculator`, deliberately lives elsewhere.)*

`GameState` has the mirror-image problem: documented as "mutable save-game data" (and `docs/architecture/high-level.md` claims "no registries or calculators"), it owns `TileEffectsContext` (a resolver over a registry) and `UnitOrderExecutor` (a service). The save/load boundary that both classes exist to define is blurred from both sides. *(Addressed 2026-07-10: this claim was simply inaccurate, not a design flaw to fix by moving `TileEffectsContext`/`UnitOrderExecutor` elsewhere — both are correctly scoped to the `WorldMap`'s lifetime, and moving them would reintroduce the same class of dangling-reference risk this finding flags elsewhere. Corrected `docs/architecture/high-level.md`'s `GameState` bullet to describe what it actually owns and why, and updated its `GameDataContext` bullet to drop the now-relocated `SecretProjectAvailabilityCalculator` line. The diagram itself is left untouched — already inaccurate in many unrelated ways per finding 7, out of scope here.)*

### 1.4 [H] The composition root is a 190-line hard-coded script

`Engine::Initialize_` (`src/game/Engine.cpp:90-273`) sequentially: loads 10 registries in a fragile implicit order, builds Lua configs, generates a hard-coded 30×20 world, hard-codes two start positions, allocates faction/base IDs from local counter variables, creates factions, wires the event bridge, builds the turn pipeline, and registers UI shortcuts. It also contains test scaffolding in the production path: an EventBus subscription labeled "for testing" (`Engine.cpp:103-115`) and a "Test setup complete" log.

Every new subsystem requires editing this method (OCP violation at the root). There is no distinction between "start application", "start new game", and "load game" — the three lifecycles this architecture will inevitably need — and no way to construct a game world for tests without replicating this sequence by hand (which `tests/GameFixtures.h` indeed does, partially and divergently: bases there get "no research/production/composition calculators", so tests exercise a different object graph than the game).

### 1.5 [H] Identity and ID allocation have no owner

- `factionId`/`baseId` are local counter variables in `Engine::Initialize_` (`Engine.cpp:204-205`); nothing else can allocate a unique ID after init.
- `Faction` does not know its own ID. The caller passes `factionId` into `Faction::CreateBase(...)` (`include/game/Faction.h:54`) and it is stored **per base** via post-construction setters (`SetFactionId`/`SetBaseId`, initialized to `-1` in the meantime — `BaseManager.cpp:45-46`).
- `GameState::GetPlayerFaction()` is defined as `m_factions[0]` (`src/game/GameState.cpp:73-81`), and the same index-0 convention is re-derived independently in `WorldView` (`k_PlayerFactionIndex = 0`) and `ViewFactory`. *(Partially addressed 2026-07-08: `WorldView` now calls `GetPlayerFaction()` and its `k_PlayerFactionIndex` is deleted; the convention is now funneled through the single `GetPlayerFaction()` definition. The denormalized-identity core — player = `m_factions[0]`, no ID owner — is untouched.)*

Denormalized identity plus convention-based "player = first" will produce subtle bugs the moment factions can be eliminated, reordered, or hot-joined, and makes serialization identity a retrofit.

### 1.6 [H] Signal/event wiring is manual, lifetime-unsafe, and non-reentrant

- `Signal<T>` (`include/lib/Signal.h`) has `connect` but **no disconnect** and no connection lifetime management. Slots capture raw references: `EventBridge::WireBase` captures `&rBase` (`src/lib/EventBridge.cpp:16-22`). Destroying any base (razing, capture) leaves dangling closures in signals that outlive it. The same applies to any future subscriber.
- `EventBus::publish` iterates `m_handlers` directly (`src/lib/EventBus.cpp:23-26`); a handler that calls `subscribe`/`unsubscribe` during dispatch invalidates the iterator (vector reallocation) — undefined behavior on a bus explicitly intended for third-party mod handlers.
- Wiring is opt-in and only the composition root does it: `EventBridge::WireBase` is called exactly once, in `Engine::Initialize_`. `Faction::CreateBase` does not wire; any base created through gameplay (colony pods) will silently emit no events. A pattern that requires every future call site to remember a manual step is a defect factory.
- Signal chains also create hidden control flow *within* one object: `BaseManager`'s constructor connects `m_pPopulation->on_growth` back into `m_pPopulation->AddPop()` (`BaseManager.cpp:72-77`) — a self-call routed through the owner via a signal, invisible to a reader of `PopulationManager`.

### 1.7 [M] Two incompatible ownership models for factions coexist

`GameState` owns `std::vector<std::unique_ptr<Faction>>`. `Diplomacy` (`include/game/faction/Diplomacy.h`) is written against `std::shared_ptr<std::vector<std::shared_ptr<Faction>>>` — a type that exists nowhere else. It is never constructed (`Faction.cpp:47` sets `m_pDiplomacy(nullptr)`), so the codebase currently carries a dead subsystem with an ownership design that contradicts the live one. Whichever is intended, one of them is wrong.

### 1.8 [H] UI holds unmanaged references to mutable game objects

- `BaseView` stores `BaseManager&` for its whole life; `GrowthDisplay`/`ProductionDisplay` store `const BaseManager*`.
- `WorldView::m_pSelectedUnit` is a raw `Unit*` (`include/ui/world/WorldView.h:57`); `UnitManager::DestroyUnit` erases the owning `unique_ptr` — the selection dangles the moment unit death exists.
- `BaseView::HandlePopClick` captures `Pop& rPop` in a lambda stored inside a popup element (`src/ui/base/BaseView.cpp:104-112`); pops are destroyed by starvation (`PopContainer::RemovePop`) while the popup can still be open.

There is no invalidation protocol (weak handle, generation ID, close-on-destroy signal) between views and game objects. Combined with 1.6, object destruction is currently unsafe almost everywhere it could occur; it "works" only because nothing dies yet.

### 1.9 [H] Turn-stage interface: nullable-pointer contract, and stages do other objects' work

- `TurnStageBase::Execute(GameState* = nullptr, Faction* = nullptr)` (`include/game/TurnStages.h:73`): default arguments on a virtual (statically bound, re-declared inconsistently in every override), forcing every stage to null-check both parameters. Per-faction and global stages share one signature — stages that ignore `pFaction` still receive it, stages that require it can be called without it (ISP/LSP smell rooted in the interface, not the implementations).
- `Execute_` is declared `private` in the base, redeclared `public` in most stages, `private` in `Population` — the NVI pattern applied inconsistently.
- Stage bodies do faction-internal bookkeeping: `IncomeCollection` and `ResearchAccumulation` contain the same loop (iterate bases by index, `ConsumeX`, sum, `AddY`) duplicated with different nouns (`src/game/stages/IncomeCollection.cpp`, `ResearchAccumulation.cpp`) — aggregation logic that belongs behind `Faction`, living in the orchestration layer, twice. *(Addressed 2026-07-09: the loops moved into `Faction::CollectIncome()`/`CollectResearch()`; the stages are now thin orchestration. The interface-level items in this finding — nullable-pointer `Execute` contract, NVI inconsistency, silent stage-ID skips, enum+switch factory — remain.)*
- `TurnProcessor::ProcessTurn` silently skips stage IDs present in `m_stageOrder` but missing from the registry (`src/game/TurnProcessor.cpp:24-26`) — a typo in `turn_stages.json` silently removes a stage from the game. Its `numFactions` parameter is unused, and `m_missionYear` is stored as a member only for a log line.
- `TurnStage` enum carries ~20 commented-out future values (`TurnStages.h:22-50`) and duplicates identity with config string IDs; `TurnStageFactory::CreateStageInstance` is a 13-case switch that must be extended for every stage (OCP): enum + switch + class + config all have to change together.

### 1.10 [H] The moddability infrastructure does not actually reach mods

Moddability is a stated core guideline, and scaffolding exists — but none of it is load-bearing:

- Hooks are parsed from `turn_stages.json` (`mod_id`, `script_path`) but `Hook::callback` is never populated from a script, and its type is `std::function<void()>` (`include/game/HookContext.h:14`) — a hook that receives no `GameState`, no faction, and no stage context could not do anything even if it were wired. Replace-hooks replace an entire stage wholesale.
- `EventBus` is the "mod-facing" API, but there is no Lua-side subscription, and `GameEvent` covers 7 event types.
- `LuaRuntime` is used solely for three cost/composition formulas.
- Player actions (set production, convert pop, assign worker, change policy) are direct method calls from UI handlers into managers (`BaseView.cpp`), bypassing anything a mod could observe or intercept; there is no command/action layer.

Individually each is "not implemented yet"; collectively they show the extension points are being built without a consumer, and the interfaces (contextless hooks, tiny event variant) would not survive first contact with a real mod. Designing them against a concrete consumer (even a sample mod) would prevent rework.

### 1.11 [M] Config validation is thorough exactly once, absent everywhere else

`ValidateEffectReferences` (`src/game/EffectReferenceValidator.cpp`) is the right idea: fail startup on any effect referencing an unknown building/tech/improvement/feature. But the standard it sets is not applied to the rest of the config surface:

- `required_techs` on buildings are never checked against `TechRegistry`.
- `SocialEngineeringManager`'s constructor hard-codes default policy IDs (`"frontier"`, `"simple"`, `"survival"`, `"none_future"` — `src/game/faction/SocialEngineeringManager.cpp:14-19`); if the config lacks them, `GetActivePolicy` silently returns `nullptr` forever.
- `BaseManager`'s constructor requires an improvement with ID `"Base"` to exist (`BaseManager.cpp:62`); if missing, `AddImprovementWithEffects` prints to `stderr` and continues — bases silently lose their tile marker.
- Tech `category` strings are validated **per query, via try/catch** in `ResearchSelector::IsTechInSelectedCategory_` (`src/game/faction/ResearchSelector.cpp:123-133`) — exceptions as control flow, and a typo'd category silently degrades selection behavior instead of failing at load.
- `SetActivePolicy(category, policyId)` never checks that the policy belongs to the category (`SocialEngineeringManager.cpp:27-35`) — any policy can be installed into any slot.
- `EnergyAllocation_t` documents "must sum to 100" and never enforces it.
- Unknown IDs in `turn_stages.json` become hook-only `CustomTurnStage`s or are silently dropped (1.9).

---

## 2. State integrity — multiple sources of truth

### 2.1 [H] Worker assignment state is spread across three classes

> **Status (2026-07-09): fixed.** Worker-assignment state now has a single owner: a
> world-scoped `WorkedTileIndex` owned by `WorldMap` (next to `UnitPositionIndex`,
> `game/map/WorkedTileIndex.{h,cpp}`), reached by every base's `WorkerAssignmentManager`
> via `WorldMap::GetWorkedTiles()`.
>
> - An assignment is a move-only RAII `WorkedTileClaim` minted exclusively by
>   `WorkedTileIndex::TryClaim(tile, bUserAssigned)` — an atomic check-and-claim that fails
>   if the tile is worked by *anyone*, so one-worker-per-tile holds across all bases and
>   **all factions** (the cross-base visibility the old `Tile` flag provided, now behind one
>   owner instead of a distributed convention). The claim releases the tile when destroyed,
>   overwritten, or cleared, so an assignment structurally cannot leak: pop death
>   (`PopContainer::RemovePop`), convert-to-non-worker (`Pop::Convert`), and reassignment
>   all free the tile with no cooperation from the call site.
> - `Pop` holds the claim instead of a bare `const Tile*` + `bool`: `SetTile`/`SetUserAssigned`
>   are gone (replaced by `SetTileClaim`, which throws if a non-empty claim is handed to a
>   non-worker type), and the user-assigned flag lives **on the claim**, so the old hidden
>   side effect ("clearing a pop's tile silently resets its user flag inside `Pop::SetTile`")
>   became a structural property: the flag cannot outlive the assignment.
> - `Tile::SetWorked/IsWorked/IsWorkerAssigned` and `mutable bool m_bWorked` are deleted —
>   this also closes 2.4's `Tile` row (the "const method mutates a mutable member" hack) and
>   2.3's "two names for the same field" bullet. `BaseWorkableAreaDisplay` now asks the base's
>   `WorkerAssignmentManager::IsTileAssigned` instead of the tile.
> - `WorkedTileIndex` owns a `Revision` bumped on every claim/release — the invalidation
>   hook 1.1 said was missing for caching `ResourceManager::ComputeWorked_` (that memoization
>   itself remains follow-up work, but is no longer blocked).
> - Guarded by `tests/game/WorkerAssignmentTests.cpp`: cross-base exclusion + release/re-claim,
>   tile freed on pop death and on convert-through-`PopulationManager` (the path that bypasses
>   `BaseManager::ConvertPop`), user-flag lifetime, workable-set/non-worker refusal, revision
>   bumps (including no bump on a failed claim).
> - Rule decided 2026-07-09 (same day, follow-up): a base may never work another base's own
>   tile. `BaseManager` claims its center tile in the index for its whole life
>   (`m_centerTileClaim`, released on base destruction). Founding a base on a tile currently
>   worked by a neighboring base's pop **displaces** the worker: the index keeps a
>   tile→claim back-map, `ClaimDisplacing` empties the worker's claim and invokes its
>   `DisplacedWorkerHandler` (registered by the owning `WorkerAssignmentManager` at claim
>   time) *after* the new claim is registered, so the worker is auto-reassigned to the best
>   free tile in its own base's radius and can never take the founding tile back. Founding
>   on another base's own tile throws (base-tile claims carry no handler). Covered by three
>   more tests in `tests/game/WorkerAssignmentTests.cpp`, including yield-based "best free
>   tile" reassignment. Repair flows (tile turns hostile, reconcile after load) remain
>   unimplemented, but displacement established the pattern they will reuse.

The canonical assignment is a `const Tile*` on `Pop`; the "is worked" flag is a `mutable bool` on `Tile` (maintained by `Pop::SetTile` and `Pop::~Pop`); the workable set and the algorithms live in `WorkerAssignmentManager`. The one-worker-per-tile invariant is maintained by convention across all three (`WorkerAssignmentManager::IsTileAssigned` reads the tile flag; the comment at `WorkerAssignmentManager.cpp:120-126` explains the choice). Additional subtlety: clearing a pop's tile silently resets its user-assigned flag as a side effect inside `Pop::SetTile` (`Pop.cpp:88-93`) — behavior callers must know but cannot see. This state model has no single owner to enforce or repair invariants (e.g., what clears assignments when a tile becomes hostile, who reconciles after load).

### 2.2 [H] Unit position is stored twice with a public bypass

> **Status (2026-07-09): fixed**, with the same ownership model as 2.1: `UnitPositionIndex`
> is the single owner of unit-position state.
>
> - **Registration is RAII**: `Unit`'s constructor takes `UnitPositionIndex&` and registers
>   the unit on its tile; the destructor unregisters. `UnitManager::DestroyUnit` (and the
>   tests that previously had to remember a manual `map.OnUnitRemoved(...)` before it) can
>   no longer leave a dangling `Unit*` in the index — the manual calls are deleted.
> - **The bypass is gone**: `Unit::SetTile` is deleted. The only way to move a unit is
>   `UnitPositionIndex::TryMoveUnit`, which updates the occupancy lists and the unit's own
>   tile pointer together (the index is a friend of `Unit`); since only the index writes
>   either side, the two representations structurally cannot desync — `m_pTile` is now a
>   cache the owner maintains, not a second source of truth. `WorldMap`'s three `OnUnit*`
>   forwarders are replaced by a `GetUnitPositions()` accessor (per 4.1's facade rule);
>   `GetUnitsOnTile` remains as a convenience forward. The old `OnUnitRemoved`
>   lookup-by-current-tile is now a `logic_error` throw if it ever misses, instead of a
>   silent no-op.
> - **Stacking rule**: the index enforces units-per-tile capacity at both entry points —
>   creation (throws) and `TryMoveUnit` (returns false; a blocked `MoveOrder` stays pending
>   and retries next turn, reroute/cancel TODO). Default is the original game's unlimited
>   stacking; `UnitPositionIndex::SetSingleUnitPerTile(true)` switches to one unit per
>   tile. The static accessor pair is an explicit stand-in until a real game-configuration
>   system exists (marked TODO).
> - Guarded by `tests/game/UnitPositionTests.cpp`: create/destroy maintain the index with
>   no manual bookkeeping, move keeps `GetTile()` and occupancy in sync, default stacking,
>   and the single-unit rule (placement throws, blocked move changes nothing, own-tile
>   move is a no-op success, destroying the blocker frees the tile) — with an RAII guard
>   resetting the static config.
> - Out of scope, still open elsewhere: `WorldView::m_pSelectedUnit` dangling on unit death
>   is 1.8 (UI invalidation protocol); `Unit`'s remaining unchecked stat setters are 4.3.

`Unit::m_pTile` and `UnitPositionIndex` both record where a unit is. `Unit::SetTile` is public; calling it directly (instead of `WorldMap::OnUnitMoved`) desyncs the index, and `UnitPositionIndex::OnUnitRemoved` then fails silently because it looks the unit up **by its current tile** (`src/game/map/UnitPositionIndex.cpp:16-24`), leaving a dangling `Unit*` in the index. The invariant "always move units through WorldMap" exists only in convention.

### 2.3 [M] Redundant flags that can desync

> **Status (2026-07-10): fixed.** All four bullets closed.

- `ResearchManager`: `m_bHasResearchTarget` duplicates `m_pCurrentResearchTarget != nullptr`; `SetResearchTarget` assigns the pointer **before** validating (`ResearchManager.cpp:28-37`), so a failed lookup throws with `m_pCurrentResearchTarget == nullptr` while `m_bHasResearchTarget` retains its old value — `HasResearchTarget() == true` with no target; `RecalculatePointsNeeded` then throws "Invalid state". *(Addressed 2026-07-10: `m_bHasResearchTarget` deleted; `HasResearchTarget()` derives from `m_pCurrentResearchTarget != nullptr`. `SetResearchTarget` now resolves into a local `pTarget` and only assigns the member after the lookup succeeds, so a failed `SetResearchTarget` on an established target leaves the prior target and flag state intact — covered by two new tests in `tests/faction/ResearchSelectorTests.cpp`.)*
- `Military`: `m_designs` vector plus `m_designMap` raw-pointer map — dual bookkeeping with no removal API yet; the first removal feature will have to remember both. *(Addressed 2026-07-10: `m_designMap` deleted. `AddDesign`/`GetDesign` both scan `m_designs` directly — a single source of truth, at the cost of O(n) lookup instead of O(1); revisit if design-list size ever makes that matter.)*
- `Faction::m_energy` lives on Faction while the class comment says the economy split is owned by `EconomyManager` — faction-level economy state is split across two objects. *(Addressed 2026-07-09: the treasury moved into `EconomyManager`; `Faction` holds no economy state.)*
- `Tile::IsWorked()` and `Tile::IsWorkerAssigned()` are two names for the same field. *(Addressed 2026-07-09 with 2.1: both methods and the field are deleted; occupancy lives in `WorkedTileIndex`.)*

### 2.4 [H] Const-correctness is systematically subverted

> **Status (2026-07-10): fixed.** All three bullets closed.

- `Tile::SetWorked(bool) const` mutates a `mutable` member, openly documented as "declared const because it is updated through a const Tile* held by Pop" (`include/game/map/Tile.h:96-99`). *(Addressed 2026-07-09 with 2.1: `SetWorked` and the `mutable` member are deleted; `Tile` has no mutable state left.)*
- `TileEffectsContext::AddImprovementWithEffects` / `RemoveImprovementWithEffects` / `RecomputeMoisture` are `const` methods that mutate tiles and trigger area-wide terrain recomputation (`include/lib/effects/TileEffectsContext.h:60-73`). *(Addressed 2026-07-10: the `const` qualifier is dropped from all three methods, and from the private `RecomputeMoistureInRadius_` helper's `TileEffectsContext&` parameter. Every call site already held a non-const `TileEffectsContext&`/reference — `BaseManager::m_rTileEffects` and the test fixtures' `ctx` — so no ripple changes were needed.)*
- `Faction`'s `GetEconomyManager() const`, `GetResearchManager() const`, `GetResearchSelector() const` return **non-const pointers** to internals (`Faction.cpp:99-102, 248-256`); `ViewFactory::CreateResearchView` extracts a mutable `ResearchManager*` from a `const Faction*`. *(Addressed 2026-07-09: replaced by const-correct reference accessors `GetEconomy()`/`GetResearch()`/`GetSocialEngineering()`; `GetResearchSelector` deleted (no callers); `ResearchView`/`CurrentResearchPanel` now take `const ResearchManager*`.)*

Individually each had a rationale; collectively they meant `const` no longer communicated immutability anywhere in the object graph. With all rows now closed, a `const` reference to any of these types can no longer be used to mutate through it.

---

## 3. Silent rule decisions and inconsistent failure handling

The coding guidelines say "Do not make up game rules or mechanics. Leave TODOs instead" and "Prefer throwing exceptions over returning default values." Several places quietly violate both:

### 3.1 [H] Energy allocation does not conserve energy

> **Status (2026-07-10): fixed.** Labs and psych take floored percentage shares;
> economy receives the remainder (SMAC residual-economy rule), so the three
> categories always sum to `totalEnergy`. `SetEnergyAllocation` now throws if the
> percentages do not sum to 100. Covered by `tests/faction/EconomyManagerTests.cpp`
> (including the 5-energy / 40-50-10 counterexample that previously minted a 6th
> energy under independent `std::round`).

`EconomyManager::CalculateEnergyForEcon/Labs/Psych` each independently `std::round` their percentage share (`src/game/faction/EconomyManager.cpp:22-35`). With 5 energy at the default 40/50/10 split: econ=2, labs=3 (round 2.5), psych=1 (round 0.5) → **6 energy allocated from 5 collected**. Rounding direction is a real game rule (SMAC defined one); here it is an emergent artifact that mints or destroys energy every turn at every base.

### 3.2 [M] Production cost of 0 today: stub value × real formula

> **Status (2026-07-10): fixed.** Three cooperating mistakes closed together:
> - Row cost is `baseCost * 10` in `ProductionCostCalculator` (static, no Lua — the
>   formula is trivial once Industry is an effect). Industry no longer appears in a formula.
> - Industry works like Growth: rating levels in `social_rating_effects.json` emit
>   `CostMultiplier` `AddPercent` effects (±10%/level), and
>   `ProductionCostCalculator::ComputeCost` resolves them from the base effect list
>   (via `BaseManager::GetMineralCost` / `ApplyProduction` → `BuildBaseEffects_`).
> - `ComputeCost` floors at `std::max(1, cost)`, matching `TechCostCalculator`.
> Covered by `tests/faction/ProductionCostTests.cpp` and the Industry rating test in
> `tests/effects/RatingTests.cpp`.

`ProductionManager::GetMineralCost` passes a hard-coded `industry_rating = 0` ("TODO: pass real industry_rating") into the shipped formula `base_cost * (10 * industry_rating)` (`config/production_cost.lua`) — every item costs **0 minerals** and completes the turn it is selected. Unlike `TechCostCalculator`, which clamps to `std::max(1, cost)`, `ProductionCostCalculator::ComputeCost` has no floor, and nothing validates formula output. Two half-finished pieces compose into degenerate live behavior with no warning anywhere.

### 3.3 [M] Growth mechanics contain three silent decisions

> **Status (2026-07-10): fixed.** All three bullets closed:
> - `GrowthCalculator`: GrowthRate ≤ 0 no longer silently becomes 1.0. Threshold-based
>   growth is blocked (`numeric_limits<int>::max()`); NearZeroGrowth / PopulationBoom
>   remain an explicit TODO (they are rule flags in SMAC, not a zero multiplier).
> - `ApplyGrowth` checks `CanGrow()` before spending the nutrient threshold / emitting
>   `on_growth`, so nutrients bank at the cap instead of paying for a phantom pop.
>   `AddPop` now throws at max size instead of silently no-oping.
> - Max base size comes from `GrowthConfig_t::maxBaseSize` (default 7, SMAC limit
>   without Hab Complex; configured in `pop_growth.json`). `SetMaxSize` remains for
>   Hab Complex / Hab Dome (TODO on the setter).
> Covered by `tests/faction/GrowthTests.cpp`.

- A growth-rate multiplier ≤ 0 is silently replaced by 1.0 (normal growth) in `GrowthCalculator::ComputeNutrientsRequired` (`src/game/population/calculators/GrowthCalculator.cpp:15-17`) — a heavy negative-growth effect stack silently does nothing.
- `PopulationManager::ApplyGrowth` deducts the nutrient threshold and emits `on_growth` *before* anyone checks whether growth is possible; `AddPop` then silently does nothing at max size (`PopulationManager.cpp:54-62, 100-115`) — nutrients are consumed for a pop that never appears, and the decision is invisible because it is split across a signal round-trip through `BaseManager`.
- Max base size is a hard-coded `m_maxSize(8)` (`PopulationManager.cpp:24`); `SetMaxSize` is never called by anything.

### 3.4 [M] Population composition hard-codes pop-type IDs

`PopContainer::ApplyCompositionTargets` converts pops to `"Drone"` and `"Talent"` by string literal (`src/game/faction/base/population/PopContainer.cpp:153, 165`), while everything else about pop types is config-driven. A mod that renames these types gets runtime throws from `ConvertTo`. Role predicates are also scattered: `IsDrone` lives on `Pop`, "is talent" is an inline expression duplicated in `PopContainer` (three places), "worker but not specialist" another.

### 3.5 [M] Tech gating semantics differ by config type and have a trap

`BuildingConfig_t::IsDiscovered` implements **OR** over `required_techs` (any one tech unlocks — surprising for a field named "required") and returns **false for an empty list** — a building with no tech requirement can never be built (`include/game/buildings/BuildingConfigParser.h:26-36`). `SocialPolicyConfig::IsAvailable` uses a *singular* `requiredTech` where empty means always available. Two config schemas, two semantics, one of them inverted for the empty case. Current data never has an empty list, so the trap is latent.

### 3.6 [M] Lua evaluation: errors return 0, "scoped" globals leak

`LuaRuntime::EvalInt` returns 0 with a console warning on any formula error (`src/lib/LuaRuntime.cpp:36-70`) — a typo in a modded formula silently zeroes tech costs or production costs. The comment says variables are "scoped to this call", but they are written as persistent globals and never cleared — formula A's variables remain visible to formula B, so a missing input in one formula silently reads a stale value from another instead of failing. Both behaviors directly contradict the project's own error-handling guideline, in the most modder-facing component.

### 3.7 [M] Social rating expansion: exact-level match silently drops out-of-table totals

`ExpandSocialRatingEffects` looks up the accumulated rating with `levelEffects.find(total)`; totals outside the configured levels produce no effects at all (`src/game/social-engineering/SocialRatingResolver.cpp:47-52`). Whether ratings clamp at the extremes is a rules decision that has been made implicitly (they vanish), without a TODO.

### 3.8 [H] Failure handling is a different style in every method

Within `BaseManager` alone: null sub-manager → throw (`GetNutrientProduction`), return 0 (`ConsumeEcon`), silent no-op (`AddBuilding`), or unchecked deref (`GetPopContainer`). Elsewhere: `Registry::Find` returns nullptr while `Registry::Create` throws; `TurnStageFactory::LoadConfig` returns `bool`; `TileEffectsContext::AddImprovementWithEffects` logs to stderr and continues; JSON parsers throw; Lua parsers warn and fall back to defaults (`ProductionCostConfig_tParser::ParseConfig`); `Faction::AddResearchPoints` silently drops points if research is null; `GetBreakthroughRate` returns `-1` sentinels; `GetDefinitionId` returns a static empty string. Callers cannot predict any failure mode without reading the callee. This is the most pervasive maintainability issue after 1.1.

---

## 4. Class design / SOLID

### 4.1 [H] `Faction` and `BaseManager` are delegation shells with god-facade interfaces

> **Status (2026-07-08 → 2026-07-09): fixed** (residual pieces tracked under 4.2/1.5/1.6).
> The population level of the stack, the owning-container leaks, and the manager facades on
> both `Faction` and `BaseManager` are resolved.
>
> Adopted rule: *a method lives on `Faction`/`BaseManager` only if it coordinates two or
> more subsystems or enforces a cross-subsystem invariant; everything else lives on the
> subsystem, reached through a reference accessor.*
>
> Done in this pass:
> - **Population API collapsed from three levels to one.** `BaseManager` no longer mirrors
>   the population surface (`GetPopContainer`, `GetPopWorkerCount`, `GetBaseSize`,
>   `GetNutrientStockpile`, `RecalculatePopComposition`, `GetDefaultWorkerTypeId`,
>   `AutoAssignWorkers` all deleted). It now exposes `PopulationManager& GetPopulation()`
>   (const/non-const) and keeps only the genuinely cross-subsystem `ConvertPop` (population
>   + worker-assignment) and `UserAssignBestAvailableWorker`. `PopulationManager` is the
>   single public surface; `PopContainer` is now an implementation detail that never escapes
>   it (`GetContainer()` removed; `WorkerAssignmentManager` and `CollectFromPops` take
>   `PopulationManager&`).
> - **Owning-container leaks closed, and positional access retired.** `GameState::GetFactions()`,
>   `Faction::GetBases()`, and both `PopContainer::GetPops()` overloads (plus
>   `PopulationManager::GetPops()`) are gone. After a short-lived interim in which they became
>   bounds-checked index accessors, the collection-level ones are now **reference ranges** —
>   `GameState::Factions()` / `Faction::Bases()` yield `Faction&` / `BaseManager&` (const
>   overloads yield `const&`) via a generic `lib/DerefView.h` adaptor over
>   `vector<unique_ptr<T>>`. A range yields references, so callers can read and mutate elements
>   but cannot reseat, move out, or destroy them — the leak stays closed while iteration reads
>   as a plain range-based `for`. There is deliberately **no** by-index accessor: an audit found
>   every caller was either iterating, asking for "the player" (now `GetPlayerFaction()`;
>   `WorldView`'s duplicate `k_PlayerFactionIndex` deleted), or the finding-4.5 first-base hack —
>   none needed random access by position, and any future by-*identity* lookup (base capture,
>   etc.) should key on the stable id, not a position that shifts when a faction is removed.
>   The same treatment was extended to pops: `PopContainer`/`PopulationManager` expose
>   `Pops()` reference ranges and `GetPop(int)` is gone. The audit confirmed no pop caller used
>   the index either — every one was forward or reverse iteration — so `WorkerAssignmentManager`
>   is now fully range-based, with the deliberate reverse pick in `UserAssignBestAvailableWorker`
>   expressed as `std::views::reverse(Pops())` to preserve which pop it selects. (This wasn't a
>   leak fix — `GetPop(int)` already returned `Pop&`, not the owning ptr — but it clears the
>   C-style-loop item in 4.3 and unifies the idiom.)
> - `AddFaction` now returns `Faction&` so callers bind the object at creation instead of
>   re-fetching it.
> - `SecretProjectAvailabilityCalculator` now takes `const GameState&` instead of borrowing
>   the raw faction vector.
>
> Done 2026-07-09 (manager facades):
> - **`Faction` subsystem accessors are const-correct references**: `GetEconomy()`,
>   `GetResearch()`, `GetSocialEngineering()` (const overloads return `const&`), replacing the
>   non-const-pointer-from-const-method getters — the `Faction` rows of 2.4 are fixed, and
>   `ViewFactory::CreateResearchView` no longer extracts a mutable `ResearchManager*` from a
>   const `Faction` (`ResearchView`/`CurrentResearchPanel` now take `const ResearchManager*`).
> - **Pass-throughs deleted**: `AddEnergy`/`GetEnergy`, `AddResearchPoints`/`GetResearchPoints`,
>   `SetSocialPolicy`/`GetSocialPolicy`/`CollectSocialEffects`; dead accessors
>   `GetResearchSelector()` and `GetAvailableSocialPolicies()` (zero callers) removed outright.
>   `m_energy` moved into `EconomyManager` (the treasury now lives with the allocation split —
>   closes 2.3's split-economy-state bullet).
> - **`BaseManager` subsystem accessors**: `GetResources()` and `GetBuildingManager()`
>   (references), `GetProduction()` (nullable pointer — see below). Deleted pass-throughs:
>   `ConsumeEcon/Labs/Psych`, `AddBuilding`/`DestroyBuilding`/`GetBuildings`,
>   `SetProduction`/`GetCurrentProduction`/`GetProductionMineralCost`/`GetMineralStockpile`,
>   `GetWorkableTilePositions`. Kept (genuine coordination): `ApplyProduction`, the
>   effects-injecting production getters, `GetConstructable`, `CollectBuildingEffects`,
>   `ConvertPop`, `UserAssignBestAvailableWorker`, `GetNutrientsRequired`, `ApplyGrowth`,
>   `ProduceResources`, `GetEffectiveSocialRating`, `GetWorkedTileYield`.
> - **1.9's duplicated stage loops moved behind `Faction`**: `CollectIncome()` /
>   `CollectResearch()` consume base stockpiles and credit the treasury / research points;
>   `IncomeCollection` and `ResearchAccumulation` are now thin orchestration. (Per-base
>   income/labs log lines were dropped in the move; stages log faction totals.)
>
> Still open / deliberate residue: ~~`GetProduction()` stays a nullable pointer~~ *(resolved
> 2026-07-09 with 4.2: production is always constructed and `GetProduction()` returns a
> reference)*. Signal forwarding on `BaseManager` (1.6) and player-index-0 (1.5) remain under
> their own findings.

`Faction` owns 12 subsystems and exposes a mix of three API styles simultaneously: pass-through methods (`AddEnergy`, `AddResearchPoints`), exposed managers (`GetResearchManager()` → callers do manager surgery), and exposed containers (`GetBases()` returning the `unique_ptr` vector). `BaseManager` repeats the shape one level down (40+ methods routing to 5 sub-managers), and `PopulationManager` repeats it again over `PopContainer` — the same population API surface exists at **three levels** (`GetWorkerCount` et al. on PopContainer, PopulationManager, and via BaseManager). Every new population feature costs three signatures plus call-site choices about which level to use; stages already use both (`GetBase(i)->ConsumeEcon()` vs `GetBases()` range loops).

SRP is satisfied by the leaf classes but the facades have become the change amplifiers. Meanwhile encapsulation is broken *around* them: `GameState::GetFactions()` and `PopContainer::GetPops()` return mutable references to owning containers, so any caller can steal or destroy objects without the facades noticing.

### 4.2 [H] Constructor-validity guideline is widely violated by two-phase initialization

> **Status (2026-07-09): fixed** for every instance listed below.
>
> - `Faction` now requires its definition as `const FactionConfig_t&` (no defaulted-null);
>   identity/AI/flavor are always constructed, `GetDefinition()` returns a reference, and the
>   `GetDefinitionId` empty-string sentinel is gone. `CreateBase` throws on a null tile.
> - `BaseManager` takes `factionId`/`baseId`/`name` in its constructor;
>   `SetFactionId`/`SetBaseId`/`SetName` are deleted. `ResourceManager` moved from a
>   constructor-body assignment into the init list (member order = dependency order), and
>   `ProductionManager` is always constructed — the constructor throws if `GameDataContext`
>   lacks a production cost calculator, and `GetProduction()` is now a **reference** (closing
>   4.1's deliberate residue). The dead defensive null checks on always-present sub-managers
>   (`m_pResources`/`m_pPopulation`/`m_pBuildings`/`m_pProduction`) are purged. The one
>   *deliberately* nullable dependency left is `pEffectsProvider` (standalone test bases),
>   now explicit at every call site instead of defaulted.
> - `GameState` is constructed with its world map + tile-effects registries;
>   `SetWorldMap`/`InitTileEffects` and their ordering throws are deleted, `GetWorldMap()`
>   returns a reference (null checks died in `ViewFactory`/`WorldView`), and `GetTileEffects()`
>   no longer needs its not-yet-initialized guard. `Engine` builds `GameState` after world gen.
> - `ResearchManager` receives its `IEffectsProvider` at construction; `SetEffectsProvider`
>   deleted.
> - `SFMLGraphics` throws from its constructor if the window failed; the vestigial
>   `Graphics::Initialize()` (and the parallel never-called `Input::Initialize()`) are removed
>   from the interfaces; `Engine::CheckInitialized_` is deleted (the constructor now validates
>   the backend factories before first use). Same treatment for `UIManager`: it takes
>   `Graphics&`/`Input&` in its constructor and its `Initialize()` two-phase setter is gone.
>
> Test fixtures now construct a real (empty-formula) `ProductionCostCalculator` over a
> `LuaRuntime`, so fixture bases run the same production-capable object graph as the game
> (partially addressing finding 8's degraded-fixture note; research/composition calculators
> are still absent there).

The project's own rule: "Constructors should accept all arguments required to make the object valid." In practice:

- `Faction`'s definition is a defaulted-null parameter; identity/AI/flavor subsystems are then conditionally null, and null-checks for them (and for always-constructed managers) spread through every method (`Faction.cpp` passim).
- `BaseManager` is born with `m_factionId = -1`, `m_baseId = -1`, then mutated by `SetFactionId`/`SetBaseId`/`SetName` (`Faction::CreateBase`).
- `GameState` requires `SetWorldMap` then `InitTileEffects` in order, throwing at runtime if misused.
- `ResearchManager` gets its `IEffectsProvider` via setter after construction (`Faction.cpp:52-55`).
- `SFMLGraphics` creates the window in its constructor while a vestigial `Initialize()` re-checks it afterwards; `Engine::CheckInitialized_` validates members at the **end** of `Initialize_`, after they have already been dereferenced.

Each instance forces defensive null/state checks downstream — which is exactly where the codebase's inconsistent failure handling (3.8) comes from. *(With the 2026-07-09 fix, several of 3.8's listed symptoms died with their causes: `GetDefinitionId`'s static empty string, `Faction`'s null-research/-flavor branches, and `BaseManager`'s throw-vs-0-vs-no-op split across always-present sub-managers.)*

### 4.3 [M] Interface hygiene

- `IGameView` is named an interface but is a concrete base with state (`m_elements`, `m_bShouldClose`) and default behavior; its `Update()` virtual is **never called by anything** (dead API implying a lifecycle that does not exist). Element-lifecycle management (erasing closed elements) happens inside `Render`.
- `IConstructable` mixes `const char* GetId()` with `const std::string& GetName()`.
- `Registry<>` has a `protected virtual Validate_` but a non-virtual public destructor — designed for inheritance and unsafe to delete polymorphically; currently safe only because all registries are held by concrete type.
- `BuildingConfig_t` (a config data struct) inherits `IConstructable`, giving every config entry a vtable and identity via `id.c_str()`; config structs also carry rule logic (`IsDiscovered`, `IsAvailable` — see 3.5). Data definitions and behavior are fused.
- `Unit` is anemic: all mutable state has public unchecked setters (`SetCurrentHp(-5)` is fine); current-vs-max semantics are undefined when faction effects change resolved `HitPoints` after spawn (`m_currentHp` snapshots the design value at construction).
- `WorkerAssignmentManager::UserAssignBestAvailableWorker` and several loops iterate by index with C-style reverse loops and duplicate their bodies (`WorkerAssignmentManager.cpp:195-224`) despite the range-based-loop guideline. *(Addressed 2026-07-08: all loops are now range-based over `PopulationManager::Pops()`, the reverse ones via `std::views::reverse`. The two remaining loops in `UserAssignBestAvailableWorker` are kept separate because their predicates and actions differ — worker-vs-specialist, one converts before assigning — not duplicated bodies.)*

### 4.4 [M] Effects pipeline: strong core, but key invariants live in comments, and one lives in a display string

The lane/seed design (`LaneFor`, `KindFor`, `SeedFor` with exhaustive switches) is genuinely good — compile-time routing that new scopes/stats cannot dodge. But:

- Grant-cycle detection parses the **UI breakdown string**: `sourceId` chains like `"a -> b -> c"` are split on `" -> "` to decide whether a building was already expanded (`GrantChainContains_`, `src/lib/effects/ActiveEffect.cpp:95-118`). A human-readable label is doing double duty as the graph-traversal data structure; any building ID containing the separator (or colliding with a segment) breaks expansion. Presentation and algorithm state must not share one string.
- The "must never enter the pool" routing rules are enforced by one wrapper type (`BaseEffects_t` — good) plus a large number of doc comments; `CollectActiveEffects(rProvider)` is a one-line passthrough adding only indirection.
- `ActiveEffect_t::config` is a raw pointer into registry storage — correct today, but nothing prevents a future registry reload (mod hot-reload) from dangling every live effect; defensive `if (!effect.config)` checks are sprinkled through consumers instead, which hides bugs rather than surfacing them.

### 4.5 [M] UI computes game values and duplicates model logic

- `GetFactionSocialRating` in the social engineering display defines a faction's rating as *base 0's* rating, with a hand-rolled fallback accumulation when no bases exist (`SocialEngineeringDisplay.cpp:184-199`) — a model concept (faction-level rating) that does not exist in the model, invented in a render file.
- `WorldView::FindBaseAtTile_` linearly scans all factions × bases per click; `Update_` rebuilds all base info structs (string copies) per frame.
- `SocialEngineeringDisplay` is 595 lines mixing layout math, display-name switch tables (`CategoryDisplayName` throwing on unknown), effect formatting, and hit-testing; `k_PoliciesPerCategory = 4` silently truncates config-driven policy lists to the UI grid size.
- Style constants (background colors, font ratios) are redefined per display file with slightly different values — no shared theme.
- `k_LeftPanelLayout` and `k_BottomLeftPanelLayout` are identical constants (`include/ui/UIElement.h:49-50`).
- Window resize rescales the SFML view, but all layouts were resolved to pixels once at startup (`ViewFactory::GetFullscreenLayout`) — post-resize hit-testing and layout drift apart.
- `UnitDesignerView` receives `nullptr /* TODO: pass UnitManager once wired into Faction */` (`ViewFactory.cpp:120`) although `Faction::GetUnitManager()` already exists — stale TODO masking an available dependency.

### 4.6 [H] Input flows through global mutable state, pumped by the graphics backend

Key/mouse events are stored in file-scope `static std::deque` globals with free push/pop functions (`src/input/KeyEventQueue.cpp:8`), **filled by `SFMLGraphics::Display() → ProcessEvents_()`** and drained by `SFMLInput`. Consequences: the Graphics and Input abstractions are secretly one system (pairing `NullGraphics` with `SFMLInput` yields dead input — the backends are not actually interchangeable, which was their entire purpose); globals preclude multiple windows and clean tests; `NullInput` blocks on `std::cin`, so the "headless" mode stalls the loop per keystroke. Also `CaptureKeyAsync` is synchronous (misnamed), and `PushPendingKeyEvent_t` applies the `_t` *type* suffix to a function. The window close button is deliberately swallowed (`SFMLGraphics.cpp` ProcessEvents_: "Ignore the close button") — an interaction decision buried in the backend.

---

## 5. Dead, unwired, and vestigial structure

Not "unimplemented features" — these are pieces that exist and are silently disconnected, which misleads readers about what the system does:

- **Tech discovery never happens in-game**: `ResearchAccumulation` adds points, but no stage or UI path ever calls `Faction::DiscoverCurrentResearch`/`ResearchManager::DiscoverTech` — points accumulate forever. The stage exists, the manager exists, the selector auto-picks targets; the loop is simply never closed. *(Stale as of 2026-07-09: `ResearchAccumulation` now runs a discover loop after collection — this predates the facade refactor and the finding should be re-verified/closed on its own.)*
- `PopulationManager::CheckRiotEndOfTurn` / `CheckGoldenAgeEndOfTurn` are called by nothing; `RiotCalculator`/`GoldenAgeCalculator` and their six signals are fully implemented dead code.
- `IGameView::Update()` — no caller. `ResearchManager::ResetAccumulatedPoints_` — no caller. `SetAccumulatedPoints`/`SetMaxSize` — no callers (public mutation APIs with no consumers).
- `ResourceManager` takes and stores `const BuildingManager* m_pBuildings` and never uses it.
- `Diplomacy` — dead class with incompatible ownership (1.7). `Specialist.cpp/h` — an empty placeholder file compiled into the target.
- `Engine::m_turnStageFactory` is a member kept alive for the whole game but used only during `Initialize_`.
- `EventBus` handler for pop events in `Engine` is test scaffolding in production.

---

## 6. Conventions, naming, and style drift

The project defines unusually specific conventions (`.devin/rules/coding-guidelines.md`); they are applied inconsistently enough that they no longer predict what code looks like:

- **`_t` postfix rule** ("Structs and Enums: postfix with _t"): roughly half comply. `FactionConfig_t`, `EnergyAllocation_t`, `Key_t` vs `TurnStageConfig`, `TurnStageInfo`, `Hook`, `WorldGenConfig`, `Color`, `SocialPolicyConfig`, `SocialRatingConfig`, all `Ev*` events, and ~14 enums (`StatId`, `TurnStage`, `ModifierOp`, `SocialCategory`, …). Worse, mechanical application produced mangled *class* names: `GrowthConfig_tParser`, `ProductionCostConfig_tParser`.
- **Function casing**: `EventBus::subscribe/publish/unsubscribe`, `Signal::connect/emit` are lowercase in a PascalCase codebase; public signal members are snake_case (`on_pop_gained`, `m_golden_age`).
- **Private-method underscore suffix** is inconsistent even within one class (`BaseView::HandlePopClick` vs `HandleTileClick_`).
- **Enum↔string mapping** is hand-rolled in at least six places (`BonusEffectParser` ×5, `ResearchCategory`, `Tile::ToString`, `SocialRatingIdToString`, UI display names) while `magic_enum` is already a dependency used by `TurnStageFactory`. Each hand map is a drift risk against its enum.
- **Config schema casing is mixed for modders**: stats snake_case (`"hit_points"`), scopes/ops PascalCase (`"ThisBase"`, `"AddPercent"`), categories lowercase (`"build"`), and numeric amounts accepted as numbers *or* strings (`"amount": "2"` in `buildings.json`, via `ParseNumber`'s `std::stod` branch) — laxity that will fragment mod configs.
- `config/buildings/buildings.json` ships a typo ID `"sattelite"` — IDs are forever once saves/mods reference them.
- 54 `std::cout`/`std::cerr` call sites in `src/game` + `src/lib` as the only diagnostics — no levels, no sink, interleaved with gameplay ("[TODO] … not yet implemented" printed to stderr at runtime from `DispatchInstantaneousEffects`). For a moddable game this will need to be an actual logging seam eventually; every new `cout` deepens the migration.
- Guideline "use references whenever possible" vs practice: constructor parameter lists of raw pointers (`Faction` takes six pointers + defaulted config; `BaseManager`, `PopContainer`, `ResourceManager` similar), `GrowthDisplay(const BaseManager*)` where the caller always has a reference. Pointers here also encode "maybe null" that then must be checked (see 4.2).

---

## 7. Documentation and build drift

- **`docs/architecture/` describes a different codebase.** `high-level.md` and `faction-system.md` reference `FactionFactory`, `HookSystem`, `TileBonusRegistry`/`config/tile_bonuses.json`, `SFMLUIManager`/`NullUIManager`, `UIWorldMap`/`UIPanel`/`UIPopup`, `BaseDisplay`, and claim Military owns Bases/BaseManager/UnitFactory — none of these exist (bases hang off `Faction`; `Military` holds only designs; the UI classes are `UIManager`/`IGameView`/`ViewFactory`). The architecting rule ("update the architecture diagrams when adding components") is not being followed; the diagrams now actively mislead, which is worse than their absence.
- `docs/game-rules/turn-structure.md` vs `TurnStages.h` commented-out enum values duplicate the same future-stage list in two places.
- `.devin/rules/build.md` says the binary is at `./build/alpha-centauri`; `CMakeLists.txt` sets `CMAKE_RUNTIME_OUTPUT_DIRECTORY` to the **source root**, so it lands at `./alpha-centauri`.
- The workable area is "21 tiles" in docs/comments and 20 (+ free center handled separately) in `MapUtils.h` — the number is right in code, the prose contradicts it.

---

## 8. Testing posture

- Coverage is effectively one subsystem deep: `lib/effects` is well tested (11 files, good fixtures), plus faction config parsing/flavor, `ResearchSelector`, and `EconomyManager` (3.1 conservation). **Zero tests** exist for: the turn pipeline (`TurnProcessor`/stages), `ResourceManager` (where the rounding and seed rules live), `WorkerAssignmentManager` (the most intricate mutable-state logic in the project), `PopulationManager`/`PopContainer` composition churn, `ProductionManager`, `BuildingManager`, `Registry`, `EventBus` (reentrancy), `WorldMap`/`Tile`, Lua calculators.
- The single test binary is named `effects-tests` while housing non-effects suites — the name will get more wrong as coverage grows.
- `tests/GameFixtures.h` constructs bases "with the minimum viable dependency set: no research/production/composition calculators" — tests run a permanently degraded object graph that the game never runs (a direct consequence of finding 4.2's optional dependencies), so integration-level behavior differences (e.g., null-production bases) are baked into the fixtures. *(Partially addressed 2026-07-09: fixture bases now carry a real, empty-formula `ProductionCostCalculator` — production is no longer optional anywhere. Research and composition calculators are still absent from fixtures.)*

---

## 9. Smaller items (recorded for completeness)

- `Registry::ValidateNoDuplicates_` is O(n²) while `m_indexById` already detects collisions during build.
- `ResearchManager` stores discovered techs as `vector<string>` with linear `HasDiscoveredTech`/prereq scans (O(T·D·P) in `GetAvailableResearchTargets`); `TechId` (a `std::string` alias) is passed and iterated **by value** in several loops.
- `Tile::HasFeature` builds two `std::string`s (via `ToString`) per call; `GetTerrainFeatureIds` allocates a vector of strings per call — both sit inside the per-tile hot paths of 1.1.
- `WorldMap` stores tiles as `vector<unique_ptr<Tile>>` (needed for address stability given `BaseManager` holds `Tile&`, but heap-per-tile for a dense grid) and exposes the owning vector mutably via `GetTiles()`.
- `ExpandGrantBuildingEffects` takes and returns the whole effects vector by value and mutates it while index-iterating (correct but fragile; growth during iteration is load-bearing).
- `ResolveStatModifiers` sorts contributions by `sourceId` purely for display determinism on every resolution, even when the breakdown is discarded.
- `AutoAssignWorkers_` erases from the front of a vector in a loop (O(n²)).
- `UserAssignBestAvailableWorker` ignores `UserAssignWorker`'s failure return — a failed steal silently does nothing.
- `NullGraphics`/`NullInput` log "Console … backend selected" — they are console backends, not null objects; naming misleads about headless capability.
- Hard-coded SFML details: window 1280×900, two distro-specific font paths, font failure degrades silently to no text.
- `include/game/faction/base/BaseTypes.h` defines `FactionId` — duplicated by the `using FactionId = int` in `lib/GameEvent.h` (two definitions of the same alias in different layers).
- `EffectContext_t` supports only `targetTile`; `ConditionSatisfied` returns false for future condition kinds via unreachable default — fine now, but the context struct will accrete fields with no versioning story for mods.
- `bd` script duplicates configure/build argument parsing between `configure`, `build`, and `all` (three copies to keep in sync).

---

## Recurring themes (the patterns behind the findings)

1. **Compute-on-read with no invalidation seam** (1.1) — the architecture has implicitly committed to "always live, always recomputed" without deciding it. *(Addressed 2026-07-09: the decision is now explicit — revision-validated memoization; see 1.1's status note.)*
2. **Invariants by convention**: manual wiring steps (WireBase), magic config IDs ("Base", "Drone", default policies), index-0 player, "call AutoAssignWorkers after", "move units via WorldMap" — each is a rule that exists only in comments and future contributors' memories.
3. **Two-phase construction + nullable dependencies** → defensive null checks → inconsistent failure styles (4.2 → 3.8): one root cause producing the two noisiest code smells.
4. **Boundaries declared but not enforced**: lib/game, data/state (GameDataContext/GameState), Null/SFML backends, "mod-facing" bus, docs vs code — in each case the stated boundary and the real dependency graph disagree.
5. **Guidelines exist but drift freely** (naming, references-over-pointers, exceptions-over-defaults, diagram upkeep) — either the rules or the code should change, but agreement is currently absent.
