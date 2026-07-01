# Effects System Review

This document records findings from a deep review of `lib/effects` and how it is wired into the
rest of the game.  Items are divided into **bugs already fixed**, **bugs that need attention**, and
**design decisions that require input**.

---

## Bugs Fixed in This Pass

### 1. `ResolveTileYieldModifiers_` — use-after-scope undefined behaviour (ResourceManager.cpp)

**Severity: high — silent data corruption / crash in optimised builds.**

Inside the anonymous-namespace helper `ResolveTileYieldModifiers_` (ResourceManager.cpp), a
`EffectConfig_t modifierConfig` was declared as a stack variable *inside* the for-loop body.  A
pointer to it was then stored in an `ActiveEffect_t` pushed to `resolvedEffects`.  That pointer
became dangling at the end of the loop iteration (stack variable lifetime ends at `}`), and
`ResolveStatModifiers` later read through it — undefined behaviour.

The same function also had a secondary logic bug: when `count == 0` (no matching improvement tiles
are worked), `Add`-type modifier amounts were left unchanged instead of being multiplied to zero,
so a building whose bonus is "+1 nutrient **per** Farm tile" would still contribute +1 even with
zero Farm tiles worked.

**Fix:** the function is rewritten to compute the total directly (same arithmetic as
`ResolveStatModifiers`) rather than constructing ephemeral `EffectConfig_t`/`ActiveEffect_t`
wrappers.  The `count == 0` case now correctly contributes 0 for `Add` operations.

### 2. `AutoAssignWorkers_` — out-of-bounds vector access (WorkerAssignmentManager.cpp)

**Severity: high — crash when more unassigned workers exist than available tiles.**

`AutoAssignWorkers_` accessed `availableTiles[0]` without checking whether the vector was
non-empty.  If the base had more worker pops than workable tiles (possible after a base shrinks or
tiles become unavailable), this was an unchecked out-of-bounds access.

**Fix:** added an `if (availableTiles.empty()) break;` guard before the access.

### 3. Dead `TileYieldModifierEffect_t` branch in `ResolveStatModifiers` (ActiveEffect.cpp)

**Severity: low — dead code, no wrong behaviour, but creates a false impression of capability.**

`ResolveStatModifiers` contained a branch that extracted `amount`/`op` from a
`TileYieldModifierEffect_t`.  This branch was never reachable in practice:

- `FilterByStatId` only returns `StatModifierEffect_t` entries, so the tile-modifier path inside
  `ResolveStatModifiers` was never exercised via the normal filter-then-resolve pipeline.
- `ResolveTileYieldModifiers_` (the only caller that passed `TileYieldModifierEffect_t` effects)
  has been rewritten to compute totals directly, removing the need for this path.

Leaving dead variant branches in a visitor-style switch/`get_if` chain is dangerous because it
invites future callers to mistakenly believe `ResolveStatModifiers` handles
`TileYieldModifierEffect_t` filtering.

**Fix:** the `TileYieldModifierEffect_t` branch is removed.  `ResolveStatModifiers` now only
handles `StatModifierEffect_t`, which is its documented purpose.

### 4. Parse-time warning for unimplemented `condition` field (BonusEffectParser.cpp)

**Severity: medium — silent correctness gap for config authors.**

`EffectConfig_t::condition` is parsed from JSON and stored, but nothing in the runtime ever
checks it.  A config author who adds `"condition": "has_tech:fusion_power"` expecting conditional
behaviour will silently get an always-on effect instead.

**Fix:** a `std::cerr` warning is emitted at parse time whenever a non-empty `condition` string is
found.  This makes the gap visible without breaking loading.

---

## Design Decisions Needed

### A. `ImprovementType` enum vs. string in `TileSelector_t`

`TileSelector_t` uses a typed `ImprovementType` enum (`Farm`, `Condenser`) to identify which tile
improvement a `TileYieldModifierEffect_t` targets.  Every other improvement reference in the system
uses plain string IDs (the `ImprovementConfig_t::id`, `Tile::HasImprovement()`, etc.).

**Current cost:**
- `ImprovementTypeToString_` in `ResourceManager.cpp` must be kept in sync with both the enum and
  the string ids in `improvements.json`.
- Adding a new improvement type (Mine, Bunker, …) for a building's tile bonus requires adding it
  to the enum, the parser (`ParseImprovementType`), and the string-conversion function — three
  places instead of one.

**Recommended change:** replace `ImprovementType`/`std::optional<ImprovementType>` in
`TileSelector_t` with `std::string` (or `std::optional<std::string>`).  Remove the enum from
`BonusEffect.h`, the `ParseImprovementType` function from `BonusEffectParser`, and
`ImprovementTypeToString_` from `ResourceManager.cpp`.  In the parser, just store the raw
improvement ID string.  In `ResourceManager`, compare against the string directly.

**Trade-off:** lose compile-time exhaustiveness checking on the selector's improvement field, gain
open extensibility without enum changes.  Given every other ID in the system is already a string,
this is consistent.

**Decision needed:** agree this is the right direction, or if the type-safety of the enum is
intentional and worth the maintenance overhead.

---

### B. `Tile::m_bonus` field is stored but invisible to the effects system

`Tile` has `SetBonus`/`RemoveBonus`/`HasBonus`/`GetBonus` and a `m_bonus` string member.  This
bonus is *not* included in `Tile::GetFeatureIds()`, so `CollectTileEffects` never looks it up in
the `ImprovementRegistry`, and it has no connection to `EffectConfig_t` whatsoever.

Meanwhile `TileBonusConfigParser` and a separate `TileBonusConfig_t` exist (with
`nutrients`/`minerals`/`energy` fields) as a parallel bonus system distinct from `EffectConfig_t`.

**Questions:**
1. Is `m_bonus` intentionally a separate "visual/lore" bonus distinct from gameplay effects?
2. Should bonus tiles eventually grant their resource bonuses via `ImprovementConfig_t` entries in
   `improvements.json`, unified with the rest of the effects system?  Or is `TileBonusConfig_t`
   the canonical approach and the effects system should not touch it?
3. If bonuses are meant to have gameplay effects, `GetFeatureIds()` should include `m_bonus` and
   a matching `ImprovementConfig_t` entry should exist for it.

---

### C. `EffectPersistence_t::Instantaneous` is parsed but never filtered

The `persistence` field is read from JSON, stored in `EffectConfig_t`, and then completely
ignored at runtime.  All effects — including those marked `Instantaneous` — are collected and
applied every turn.

For `GrantBuilding`/`GrantTech`/`GrantUnit` effects the expected semantics is "fire once when
built", not "fire every turn".  Currently `GrantBuilding` expansion happens inside
`CollectActiveEffects` which is called every turn — this works correctly only because the
result (a building's effects) is stateless (the building either exists or doesn't).  But
`GrantTech`/`GrantUnit` fire-once semantics will be wrong the moment those effects are consumed
somewhere.

**Options:**
1. **Remove `Instantaneous` for now** — delete the enum value, throw on parse, and address it
   when the first user of fire-once semantics is implemented.
2. **Implement a simple gate** — effects with `Instantaneous` persistence are only applied by a
   separate `ApplyInstantaneousEffects(faction)` call triggered at construction/event time, never
   by `CollectActiveEffects`.  This would require a dispatch path in the `Engine` turn stages.
3. **Keep as is with a warning** — similar to the condition field, emit a parse-time warning if
   `Instantaneous` is set, since its behaviour differs from expectation.

**Decision needed:** which option, and whether any current config data already uses
`Instantaneous` that would break under option 1.

---

### D. `FactionGlobal`/`WorldGlobal` `GrantBuildingEffect_t` loses per-base attribution

(Already documented in the architecture doc's Known Gaps section.  Repeated here for completeness.)

When a faction-wide effect grants a building, the granted building's `ThisBase`-scoped sub-effects
inherit `originBase = nullptr` and are silently dropped by `FilterForBase` for every base.  No
current data exercises this, but the first secret project that grants a building with per-base
bonuses will silently no-op those bonuses.

**Potential fix:** when expanding a `GrantBuilding` effect inside `CollectFromBuildings`, if the
granting effect is `AllOwnerBases`-scoped, clone the granted building's effects once per base
(with each base as `originBase`).  If it is `FactionGlobal`-scoped and the sub-effect is
`ThisBase`-scoped, the intent is ambiguous — it could mean "grant to all bases" or "grant
faction-wide and ignore per-base sub-effects".

**Decision needed:** intended semantics for faction-wide `GrantBuilding`, so the expansion logic
can be made correct before the first secret project that uses it.

---

### E. `ResolveStatModifiers` double-dispatch will accumulate as effect types grow

`ResolveStatModifiers` now only handles `StatModifierEffect_t` (after the dead
`TileYieldModifierEffect_t` branch was removed).  The function is passed pre-filtered
`ActiveEffect_t` vectors, so it's clean.

The separate inline math in the rewritten `ResolveTileYieldModifiers_` duplicates the
`addTotal / arithmeticFactor / geometricFactor` accumulation.  There are now two places that
implement the "add all adds, then apply arithmetic factor, then geometric factor" formula.

**Options:**
1. **Leave as is** — the duplication is small, both sites are in the same file, and keeping
   `ResolveStatModifiers` focused on `StatModifierEffect_t` keeps the public API clean.
2. **Extract the formula into a free function** — e.g. a helper
   `double ApplyModifierStack(double base, span<(amount, op)> contributions)` that both callers
   use.  This ensures the formula stays in sync but adds a new internal function.

**Decision needed:** whether consistency of the formula or API cleanliness is the priority.

---

## Minor Issues (No Decision Needed, Can Be Fixed Anytime)

### M1. Max radius re-scanned on every `CollectAreaEffects` call

`AppendAreaEffectsFromNeighbors_` in `TileEffectsContext.cpp` iterates all improvement configs to
find `maxRadius` every time any tile's area effects are collected.  For a map with many tiles and
a large neighbour scan per tile, this is O(numImprovements) overhead per tile per call.

**Fix:** cache `maxRadius` in `ImprovementRegistry` (or precompute it once in
`TileEffectsContext`'s constructor) and update it when configs are loaded.  This is pure
performance with no behaviour change but requires a small addition to `ImprovementRegistry` or
`TileEffectsContext`.

### M2. `UnitBonusTableEffect_t::value` uses `float` while the rest of the system uses `double`

The field is `float value` while `StatModifierEffect_t::amount` is `double`.  The parser does
`static_cast<float>(ParseNumber(...))`.  `ResolveBonusTable_` in `UnitDesign.cpp` returns
`unordered_map<string, float>`.  Changing to `double` would require changing that return type and
all callers, for a negligible precision gain (terrain bonuses are all small integers in current
data).  Low priority; worth unifying if the bonus table is ever used for non-integer values.

### M3. `CanBuildImprovement` allocates a vector on every call

`Tile::GetFeatureIds()` returns a `std::vector<std::string>` by value.  `CanBuildImprovement`
calls it and searches the result.  If this function is ever called in a tight loop (e.g., for
every tile when the player opens a build menu), the allocation becomes visible.  A simple fix is
to cache `GetFeatureIds()` in the tile or provide an `HasFeature(string_view)` helper that avoids
the allocation.

### M4. `std::set` key in `CollectFromBuildings` uses raw pointer comparison

The deduplication key `{const BaseManager*, std::string}` relies on pointer identity for the
origin base.  This is intentional (two different bases granting the same building should each
expand independently), but it means the set key is tied to `BaseManager` object lifetime.
Since `CollectFromBuildings` runs in the same stack frame as the `Faction`'s base list, the
pointers are always valid.  Worth a comment to make the invariant explicit.

### M6. `GameState` member destruction order *(fixed)*

`GameState` members are destroyed in reverse declaration order:
1. `m_pTileEffects` (destroyed second-to-last)
2. `m_worldMap` (destroyed third-to-last)
3. `m_factions` (destroyed last — includes all `BaseManager` instances)

`BaseManager` holds `TileEffectsContext& m_rTileEffects` which refers to the already-destroyed
`m_pTileEffects`.  Currently this is harmless because no destructor in the base hierarchy uses
the tile effects context.  But the moment any teardown path calls
`RemoveImprovementWithEffects` (e.g. a future `BaseManager::~BaseManager` that removes the
`"Base"` improvement) the reference will be dangling.

**Fixed:** reordered the members in `GameState.h` so `m_factions` is declared after
`m_worldMap` and `m_pTileEffects`, ensuring bases are destroyed before the context they
reference.

### M7. `ResourceManager::Get*Production()` uses stale effects before first `ProduceResources` *(fixed)*

`m_activeEffects` is initialised as an empty vector.  The live UI queries
(`GetNutrientProduction` etc.) use this cached vector, so before the first call to
`ProduceResources` (which happens during the first ResourceCollection turn stage), any UI that
displays production numbers will show building bonuses as zero even though the buildings exist.
Tile-based production is computed correctly (it reads live from workers), but
`StatModifier`/`TileYieldModifier` effects from buildings are absent.

**Fixed:** `Engine::Initialize_` now calls `ProduceBaseResources()` on every faction after
setup, so the UI shows correct production figures from the first frame.

### M8. `ComputeWorkedResources` is called up to 3× per turn per base

`CalculateNutrients_`, `CalculateMinerals_`, and `CalculateEnergy_` each call
`CalculateResourceWithTileYieldModifiers_`, which calls `workerAssignments.ComputeWorkedResources()`
at the top.  The three calls are independent and each iterates all workers, calling
`ResolveTileYield` and `ApplyTileMultipliers` per worker.  For a base with 6 workers that is 18
`ApplyTileMultipliers` calls per turn (each allocating filtered effect vectors).

**Fix option:** compute `worked` once in `ProduceResourcesInternal_` or pass it in.  Not
critical at current scale.

### M5. `Tile::AssignWorker(int baseId)` / `UnassignWorker()` are parallel to `SetWorked(bool)`

`Tile` has two separate "worked" tracking mechanisms: `m_bWorked`/`m_workedByBaseId` (set by
`AssignWorker`/`UnassignWorker`) and `m_bWorked` again via `SetWorked(bool)` (set by `Pop`
through a `mutable` field).  The `m_workedByBaseId` field appears to be unused downstream — no
code queries `GetWorkedByBaseId()` for gameplay logic.  Consider removing the `AssignWorker` /
`UnassignWorker` / `GetWorkedByBaseId` path from `Tile` if `Pop::SetWorked` is the authoritative
source, or document why both paths exist.
