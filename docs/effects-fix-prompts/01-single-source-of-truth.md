# Package 1 — Single source of truth for base effects

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 1; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** Confirm all three [H] findings. Confirm the review’s fix themes, with amendments on *where* world/council fold in (compose at `Faction`/`IEffectsProvider`, not inside `FactionEffectsPool::Rebuild_`) and on rounding (centralize `lround`; also replace ad-hoc casts in resource/pop paths that share the “one rounding rule” theme).

---

## Verified diagnosis

### 1. Two divergent effect lists per base — **confirmed**

Memoized / UI / production-cost paths use provider-only `BuildBaseEffects_()`:

```300:314:src/game/faction/base/BaseManager.cpp
const BaseEffects_t& BaseManager::BuildBaseEffects_() const
{
    // ...
    const FactionEffects_t& rPool = m_pEffectsProvider->GetActiveEffects();
    const uint64_t poolVersion = m_pEffectsProvider->GetEffectsVersion();
    // ...
    m_cachedBaseEffects = BuildBaseEffects_(rPool);
    // ...
}
```

Call sites that go through that memo alone include production completion and mineral cost (`BaseManager.cpp:266-273`), the five live production getters (`:194-217`), `GetNutrientsRequired` (`:348-350`), `GetWorkedTileYield` / `GetPreviewTileYield` / `GetBaseEffects` (`:363-375`).

Turn stages append a strictly larger pool for banked resources and growth:

```18:26:src/game/stages/ResourceCollection.cpp
// Other factions' WorldGlobal effects apply here too (the faction's own pool
// already includes its own).
const std::vector<ActiveEffect_t> worldEffects = rGameState.CollectWorldEffects(rFaction);
rFaction.ProduceBaseResources(worldEffects);
```

```23:25:src/game/stages/Population.cpp
const std::vector<ActiveEffect_t> worldEffects = rGameState.CollectWorldEffects(rFaction);
rFaction.ApplyBaseGrowth(worldEffects);
```

```455:480:src/game/Faction.cpp
void Faction::ProduceBaseResources(const std::vector<ActiveEffect_t>& rExternalEffects)
{
    FactionEffects_t factionEffects = GetActiveEffects();
    factionEffects.effects.insert(..., rExternalEffects...);
    pBase->ProduceResources(factionEffects);  // non-memo BuildBaseEffects_(pool)
}
// ApplyBaseGrowth — same pattern
```

`GameState::CollectWorldEffects` (`GameState.cpp:119-142`) gathers other factions’ `WorldGlobal` plus council world laws and governor faction-globals. `BaseProduction` (`BaseProduction.cpp:33`) calls `ApplyProduction()` → memo path **without** those extras, so mineral cost can disagree with minerals banked in `ResourceCollection`. `GrowthDisplay` (`GrowthDisplay.cpp:46`) calls `GetNutrientsRequired()` (memo) while `ApplyGrowth` used the enriched pool — same-turn UI vs turn resolution divergence.

Own-faction `WorldGlobal` is already inside `FactionEffectsPool` (verified by `UniversalRoutingTests.cpp:213-228`); the hole is **cross-faction WorldGlobal + council**, which never enter the provider version the memo keys on.

Memo invalidation for *local* inputs is fine: key is pool version; `FactionEffectsPool::CollectRevisions_` (`FactionEffectsPool.cpp:117-129`) includes per-base building and population revisions. The bug is membership of the pool, not stale keying of the local stamp.

Architecture drift: `effects-system.md:355` claims `ProduceResources` / `ApplyGrowth` / `GetNutrientsRequired` share `BuildBaseEffects_` via the provider — aspirational; code has the dual path. `effects-system.md:343` claims `GrowthDisplay` passes the faction pool into `GetNutrientsRequired`; it does not (`GrowthDisplay.cpp:46` → no-arg getter → memo).

### 2. Base-level percentage modifiers silently discarded — **confirmed**

```119:126:src/game/faction/base/resources/ResourceManager.cpp
int ResourceManager::CalculateResource_(StatId_t stat, const TileResources_t& worked,
                                        const BaseEffects_t& rBaseEffects) const
{
    double base = static_cast<double>(GetResourceValue_(worked, stat));
    base += ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, stat), SeedFor(stat)).total;
    return static_cast<int>(base);
}
```

`SeedFor` for `Nutrients`/`Minerals`/`Energy` is `0.0` (`EffectEnums.h:96-98`, `:140`). `ApplyModifierStack` (`ActiveEffect.cpp:218-232`) does `(seed + adds) * arithmeticFactor * geometricFactor`, so an `AddPercent` on seed 0 vanishes; adding that 0 onto `worked` leaves the percent unused.

Same shape for energy splits:

```139:162:src/game/faction/base/resources/ResourceManager.cpp
return m_pEconomy->CalculateEnergyForEcon(energy)
     + static_cast<int>(ResolveStatModifiers(..., SeedFor(StatId_t::Econ)).total);
// Labs / Psych identical
```

Shipped data: Economy rating levels 2/3 declare selector-free `minerals` `AddPercent` −10/−20 (`config/social_rating_effects.json:19,23`). Tile path only forwards **selector-carrying** modifiers (`TileEffectsContext.cpp:188-204`), so those penalties apply nowhere today.

Correct pattern already exists on pops:

```107:111:src/game/population/pop-types/Pop.cpp
ResolveStatModifiers(FilterByStatId(tileEffects, statId), static_cast<double>(rawValue));
```

### 3. Two int-rounding policies for the same stack — **confirmed** (example amended)

Every `ResolveStat` overload truncates:

```473:531:src/game/effects/ActiveEffect.cpp
return static_cast<int>(ResolveStatModifiers(...).total);
```

Combat and council use `std::lround` on the same `ApplyModifierStack` math:

```218:233:src/game/units/MoraleCalculator.cpp
return static_cast<int>(std::lround(ApplyModifierStack(SeedFor(statId), contributions)));
```

```173:175:src/game/council/PlanetaryCouncil.cpp
return std::max(0, static_cast<int>(std::lround(breakdown.total)));
```

**Amendment to the chassis example:** Infantry’s `+25%` attack (`config/unit_components/chassis.json:16-21`) carries `condition: TargetTileHas Base`, so context-free `GetStat(Attack)` skips it via `FilterByStatId`. The fork is live when both paths use context (`ResolveStat(unit, Attack, ctx)` vs `ResolveCombatStat`) against a strength-2 additive base: `2 * 1.25 = 2.5` → truncate **2** vs `lround` **3**. Air `movement -50%` (`specials.json:66-69`) similarly splits odd totals (`1.5` → 1 vs 2).

**Additional fork (same package theme):** `Pop::ApplyTileMultipliers` uses `std::round` (`Pop.cpp:111`); `ResourceManager::CalculateResource_` uses `static_cast<int>` (`ResourceManager.cpp:126`). Three policies for one stack vocabulary.

`ResolveAdditiveStat` (`ActiveEffect.h:314-316`) intentionally ignores percent ops for SMAC-style base ratings — leave that contract alone; do not funnel it through percent-aware rounding changes beyond shared float→int if it still returns an int from a double add-total.

---

## Design decision

### Chosen

**A. Compose world/council into the provider surface Faction exposes, not into `FactionEffectsPool::Rebuild_`**

1. Keep `FactionEffectsPool` as the **local** faction assembly (package 2 owns expand/order). Do not teach the pool about `GameState`/council.
2. Bind a session world-effects source onto `Faction` (mirror `BindWorldMap` / `SetSettings`): GameState implements collection equivalent to today’s `CollectWorldEffects`, but cross-faction harvest must read **local** pools only (else composed `GetActiveEffects` would double-count council when scanning peers).
3. `Faction::GetActiveEffects()` / `GetEffectsVersion()` become the composed view: local pool + world/council extras for that faction. Memoize the composed `FactionEffects_t`; version mixes local pool version with a world stamp (council `PlanetaryCouncil::GetRevision()` plus other factions’ **local** pool versions, or an equivalent GameState revision that moves when either changes).
4. Delete the divergence API:
   - `BaseManager::ProduceResources(const FactionEffects_t&)` / `ApplyGrowth(const FactionEffects_t&)` → no-arg methods that always use `BuildBaseEffects_()` (memo).
   - `Faction::ProduceBaseResources` / `ApplyBaseGrowth` take no external vector; stages call them with no `CollectWorldEffects` append.
5. Keep `GameState::CollectWorldEffects` as the implementation behind the world source (or fold into that type); update call sites/tests that currently pass the vector explicitly (`UniversalRoutingTests`, `RatingTests`, `SupplyCrawlTests`).

`CollectLiveUnitEffects` only pulls `FactionUnits` / `ProducedAtThisBase` from the pool (`ActiveEffect.cpp:443-450`), so composing WorldGlobal/council into `GetActiveEffects` does not leak into unit stats. `FilterForBase` already includes `FactionWide` (`ActiveEffect.cpp:329`), which is what bases need.

**B. Seed base-level resolve with the value being modified**

- `CalculateResource_`: `ResolveStatModifiers(filter, static_cast<double>(workedVal)).total` — do **not** add worked then resolve against 0.
- `CalculateEcon_` / `Labs_` / `Psych_`: seed with the energy-split integer, then finalize once (same pattern).

**C. One float→int convention: `std::lround`, owned next to `ApplyModifierStack` / `SeedFor`**

- Add something like `FinalizeResolvedStat(double)` (name per coding guidelines) in the effects headers; document that half-away / `lround` is the game-wide rule for resolved modifier totals.
- All `ResolveStat` overloads use it.
- `MoraleCalculator::ResolveCombatStat` and `PlanetaryCouncil` vote weight call the same helper (combat may still append morale `AddPercent` before finalize).
- Align `ResourceManager` and `Pop::ApplyTileMultipliers` int conversion with the same helper so base economy and tile multipliers do not reintroduce a fork.
- Treat test updates that asserted truncate as **requirement changes**, not weakenings.

### Rejected

| Alternative | Why not |
|-------------|---------|
| Merge world/council inside `FactionEffectsPool::Rebuild_` | Couples pool rebuild to session/council; fights package 2; risks Faction↔GameState cycles; blurs local stamp semantics. |
| Inject `GameState` into every `BaseManager` and merge only in `BuildBaseEffects_` | Fixes memo consumers but leaves `GetActiveEffects()` meaning split; duplicates composition; heavier leaf dependency. |
| Keep stage-appended overloads “for tests” | Re-invites divergence; coding guidelines forbid back-compat shims — update tests. |
| Truncate everywhere (match `ResolveStat` today) | Combat/council already `lround`; player-visible combat and SMAC-like +25% on odd bases want round-half, not silent truncate. |
| Seed Economy mineral % into the tile pass | Selector-free base modifiers belong at base level; tile pass is for selector-carrying mods only (architecture + `AppendMatchingTileModifiers_`). |

### Interactions with packages 2–8

| Package | Interaction |
|---------|-------------|
| **2** Pool rebuild | Must **not** absorb world composition into `Rebuild_`. May need `Faction::GetLocalActiveEffects()` / local version so package 2 and world harvest share a clear local surface. Coordinate if both touch `IEffectsProvider` / `Faction`. |
| **3** ActiveEffect contracts | Session wiring for world bind is adjacent to possible `GameState` injection for Infiltration — prefer the same Bind* style, don’t merge the PRs. |
| **5** Parser | No dependency. |
| **6** Tile yield | Inherits finalize/rounding if tile resolve goes through shared helpers; preview/worked unification stays in package 6. Optional `ResolveStatModifiers` sort cost stays out of scope here. |
| **7–8** | No blockers. |

Docs to update when implementing: `docs/architecture/effects-system.md` (WorldGlobal collection row `:108`, `:219`; ResourceManager seed semantics `:356-358`; GrowthDisplay claim `:343`; ResourceManager “single BuildBaseEffects_” story `:355`; document `FinalizeResolvedStat` next to `ResolveStatModifiers` / `SeedFor`).

---

## Implementation plan

1. **World composition API**
   - Add a narrow session surface (interface or GameState bind) that returns the extras currently built by `CollectWorldEffects`, reading peer **local** pools + council.
   - `Faction::BindWorldEffects(...)` from `GameState::AddFaction` (alongside `BindWorldMap`).
   - Compose + cache in `Faction::GetActiveEffects` / `GetEffectsVersion`; expose local-only accessors for harvest and for package 2 if needed.
2. **Delete divergent overloads**
   - `BaseManager`: no-arg `ProduceResources()` / `ApplyGrowth()` → `BuildBaseEffects_()` only.
   - `Faction`: no-arg `ProduceBaseResources()` / `ApplyBaseGrowth()`.
   - Stages: drop `CollectWorldEffects` append; call no-arg Faction methods.
   - Update tests that pass `{}` or explicit world vectors.
3. **Base-level % seed**
   - Fix `CalculateResource_` and `CalculateEcon_`/`Labs_`/`Psych_` to seed with the value being scaled; single finalize to int.
4. **Rounding**
   - Introduce shared finalize helper; switch `ResolveStat*`, combat, council votes, ResourceManager, Pop tile multipliers.
   - Golden tests: strength-2 + 25% → 3; movement odd×50% → documented `lround` result; Economy rating minerals −10%/−20% reduces banked and live `GetMineralProduction`.
5. **Invalidation checks**
   - Changing council revision or another faction’s local WorldGlobal must bump composed version so `BaseManager` memo rebuilds; UI getters and turn banking agree.
6. **Architecture doc** sync for the above boundaries.

---

## Test plan

Requirement-based (assert intended rules, not today’s wrong answers):

1. **One pool — WorldGlobal cross-faction**  
   Faction A builds a WorldGlobal energy source; faction B’s `GetEconProduction()` / `GetMineralProduction()` (memo getters) match banked `ProduceBaseResources()` for the same turn state (extend `UniversalRoutingTests` pattern).

2. **One pool — council / governor**  
   With an active council world (or governor faction-global) effect that changes growth threshold or minerals, `GetNutrientsRequired()` / production getters equal the values used by `ApplyGrowth` / `ProduceResources` without passing an external vector.

3. **Memo invalidation**  
   Activate/change a council law (or peer WorldGlobal); base memoized mineral/nutrient queries change without requiring a local building/pop mutation on the observing base.

4. **Economy minerals AddPercent**  
   Force Economy rating level 2 (or inject the same selector-free `minerals -10%` base-level effect): worked minerals 10 → resolved production 9 (and level 3 → 8). Live getter and `ProduceResources` bank agree. Pure `Add` flat minerals still add after / as part of the same seeded stack correctly.

5. **Econ/Labs/Psych AddPercent**  
   Base-level `+50%` Labs on a known energy split seeds from the split, not from 0 (facility-style future case).

6. **Rounding — combat vs GetStat**  
   With context that admits chassis/weapon `+25%` on additive attack 2: both `ResolveStat(..., ctx)` and `ResolveCombatStat` (modulo morale extras) use `lround` on the shared stack; document expected 3. Update `MoraleCalculatorTests` if the relationship to truncated `baseAttack` was pinning the bug.

7. **Rounding — movement**  
   Odd movement total with `AddPercent -50%` resolves via `lround` (requirement), not truncate.

8. **Council votes**  
   Existing vote-weight tests still pass through the shared finalize helper.

9. **No double-count**  
   Faction A’s own WorldGlobal still appears once for A (composed path must not append A’s WorldGlobal again).

10. **Lane typing**  
    Still impossible to pass `FactionEffects_t` into `FilterBaseLevelByStatId` / ResourceManager; `BaseEffects_t` remains the only base-level list.

---

## AI implementation prompt

```markdown
# Implement Package 1 — Single source of truth for base effects

You are working in the Alpha Centauri C++ rebuild at `/home/martok/alpha-centauri`.

## Goals

1. **One effect pool for every base consumer.** Memoized `BaseManager::BuildBaseEffects_()`, live production getters, mineral cost / `ApplyProduction`, nutrient threshold / UI, tile yield queries, and turn-stage `ProduceResources` / `ApplyGrowth` must all resolve against the same composed faction pool (local `FactionEffectsPool` **plus** other factions’ `WorldGlobal` **plus** planetary council world/governor extras). Delete APIs that take a caller-supplied `FactionEffects_t` / external effect vector for those paths.

2. **Base-level `AddPercent` must scale the value being modified.** `ResourceManager::CalculateResource_` and `CalculateEcon_`/`Labs_`/`Psych_` must seed `ResolveStatModifiers` with the worked resource or energy-split value (same idea as `Pop::ApplyTileMultipliers`), not `SeedFor(stat)` (0) added on top. Shipped Economy rating minerals −10%/−20% in `config/social_rating_effects.json` must affect production.

3. **One float→int rule for resolved modifier totals.** Introduce a shared helper next to `ApplyModifierStack` / `SeedFor` that uses `std::lround`. Use it from all `ResolveStat` overloads, `MoraleCalculator::ResolveCombatStat`, `PlanetaryCouncil` council-vote finalization, `ResourceManager` int conversion, and `Pop::ApplyTileMultipliers`. Do not leave truncate vs `lround` vs `std::round` forks on the same stack math.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md`: SOLID, references over pointers, throw over silent defaults, no legacy/back-compat shims — update all call sites and tests.
- Build and test **only** via `./bd` (never raw cmake/make/ctest).
- Prefer config/Lua for game numbers; do not hardcode balance constants in C++.
- Do **not** change Engine unless truly required (it should not be).
- Do **not** implement package 2 (pool expand order / grant dedupe / social-rating accumulate), package 5 parser work, or package 6 tile-preview unification — except unavoidable call-site updates.
- Preserve `FactionEffects_t` / `BaseEffects_t` lane typing.
- Do **not** fold world/council collection into `FactionEffectsPool::Rebuild_`. Compose at the `Faction` / `IEffectsProvider` boundary (or an equivalent session bind), keeping the pool local for package 2.
- When harvesting other factions’ `WorldGlobal` for composition, read **local** pools only so council extras are not double-counted.
- `CollectLiveUnitEffects` must keep pulling only `FactionUnits` / `ProducedAtThisBase` (composing WorldGlobal into `GetActiveEffects` is fine because of that filter).
- `ResolveAdditiveStat`’s “Add only, ignore percent” contract stays.
- Update `docs/architecture/effects-system.md` for WorldGlobal collection, ResourceManager seed semantics, single `BuildBaseEffects_` entry, and the rounding helper.

## Analysis reference

Read and follow: `docs/effects-fix-prompts/01-single-source-of-truth.md` (verified diagnosis + design). Findings origin: `docs/effects-model-review.md` (divergent pools; base-level %; rounding).

## Primary files

- `include/game/IEffectsProvider.h`
- `include/game/Faction.h`, `src/game/Faction.cpp`
- `include/game/faction/FactionEffectsPool.h`, `src/game/faction/FactionEffectsPool.cpp` (local-only accessors if needed — not world merge in Rebuild_)
- `include/game/GameState.h`, `src/game/GameState.cpp` (`CollectWorldEffects` / bind)
- `include/game/faction/base/BaseManager.h`, `src/game/faction/base/BaseManager.cpp`
- `src/game/faction/base/resources/ResourceManager.cpp`
- `include/game/effects/ActiveEffect.h`, `src/game/effects/ActiveEffect.cpp`
- `include/game/effects/EffectEnums.h` (document finalize next to `SeedFor` if that is the chosen home)
- `src/game/units/MoraleCalculator.cpp`
- `src/game/council/PlanetaryCouncil.cpp`
- `src/game/population/pop-types/Pop.cpp`
- `src/game/stages/ResourceCollection.cpp`, `Population.cpp`, `BaseProduction.cpp` (call sites)
- `docs/architecture/effects-system.md`
- Tests: `tests/effects/UniversalRoutingTests.cpp`, `ModifierMathTests.cpp`, `RatingTests.cpp`, `tests/game/MoraleCalculatorTests.cpp`, council vote tests, resource/production tests as needed; add Economy minerals % coverage

## Acceptance criteria

- [ ] No `ProduceResources`/`ApplyGrowth`/`ProduceBaseResources`/`ApplyBaseGrowth` overload remains that accepts an external effect list for “enriched” resolution.
- [ ] Turn stages do not manually append `CollectWorldEffects` before production/growth.
- [ ] For a base in a multi-faction game with peer `WorldGlobal` and/or council effects: live getters (`Get*Production`, `GetNutrientsRequired`, `GetMineralCost`) match turn banking/growth resolution for the same state.
- [ ] Composed effects version moves when council revision or peer local WorldGlobal inputs change, invalidating `BaseManager` memo.
- [ ] Economy rating minerals `AddPercent` reduces mineral production (getter + bank).
- [ ] `ResolveStat` and combat/council finalize with the same `lround` helper; golden case additive 2 × +25% → 3 when the percent applies.
- [ ] `./bd test` passes for affected suites (effects, faction/base resources, morale, council as touched).
- [ ] Architecture doc updated; no new back-compat shims.

## What NOT to do

- Do not put world/council merging inside `FactionEffectsPool::Rebuild_`.
- Do not “fix” divergence by making UI also call the stage-only overload.
- Do not seed base-level percent resolve with 0 and multiply elsewhere as a special case.
- Do not change package 2 expand order, grant dedupe, or social-rating faction-lane accumulation rules except as required for local-vs-composed accessors.
- Do not unify tile preview vs worked yield (package 6).
- Do not relax or delete tests to hide truncate→lround or % seed requirement changes — update assertions to the new requirements.
- Do not invent SMAC rules beyond what config/docs already express; leave true unknowns as TODO.
```
