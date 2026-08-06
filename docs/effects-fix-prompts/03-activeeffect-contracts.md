# Package 3 — Origin tagging & ActiveEffect API contracts

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 3; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** Confirm the package’s fix direction with small wiring amends (Faction session back-pointer for `GameState`; stable council config storage rather than “never retain”).

---

## Verified diagnosis

Claims checked against current tree (path:line).

### Origin tagging has a second, hand-maintained implementation — **confirmed**

- `AppendActiveEffectsIf_` is documented as the single config→`ActiveEffect_t` conversion (`ActiveEffect.h:116-120`) and derives tagging from `LaneFor` (`ActiveEffect.cpp:57-61`): Base lane, ProducedAtBase lane, or `FactionUnits`.
- `BuildingManager::CollectEffects` passes `nullptr` as origin (`BuildingManager.cpp:63`) and documents that the caller re-tags (`BuildingManager.h:36-38`).
- `BaseManager::CollectBuildingEffects` re-stamps with a hardcoded scope triple (`BaseManager.cpp:233-242`): `ThisBase || ProducedAtThisBase || FactionUnits` — a copy of the `LaneFor` rule, not derived from it.
- Architecture already states base-anchored sources must pass the base into `AppendActiveEffects` (`docs/architecture/effects-system.md` “Adding a new effect source” / Collection helpers), but buildings violate that contract today.

Same finding appears twice in the review (core slice + base-management cross-slice); one fix covers both.

Related (same pattern, include in this package): `CollectFromPops` calls `CollectPopEffects` → `AppendActiveEffects(..., nullptr, ...)` then post-assigns `originBase` (`ActiveEffect.cpp:364-383`). Prefer tagging at append time with `&rOriginBase`.

### `DispatchInstantaneousEffects` drops Infiltration — **confirmed**

- Signature defaults `GameState* pGameState = nullptr` (`ActiveEffect.h:383-384`).
- Sole production caller omits it (`BaseManager.cpp:113`).
- Infiltration branch prints `[TODO]` to stderr and `continue`s when null (`ActiveEffect.cpp:428-435`); when non-null it calls `ApplyInfiltrationEffect` (`:436`), which needs full `GameState` (ledger + faction walk + council membership via `FactionFilterCoversTarget` — `InfiltrationRules.cpp:74-94`, `:47-72`).
- Continuous Infiltration is real in shipped/fixture data (`config/council/rules.json:12-16`; probe fixtures); Instantaneous on buildings is latent (no shipped building declares it yet) but the only production-completion dispatch site will silently no-op the first one that does.

### `ActiveEffect_t::config` null checks / no constructor invariant — **confirmed**

- `ActiveEffect_t` is an aggregate with `const EffectConfig_t* config` and no constructor (`ActiveEffect.h:30-41`).
- Production path always sets `&rEffect` in `AppendActiveEffectsIf_` (`ActiveEffect.cpp:52-53`).
- Four `ResolveFlag` overloads each open with `if (!rEffect.config) continue;` (`ActiveEffect.cpp:503-518`, `:551-566`, `:568-583`, `:585-600`) with near-identical bodies; filters and `ResolveStatModifiers` also null-check (`ActiveEffect.h:194-197`, `:239-242`, etc.).
- Tests intentionally pin null-tolerance: `ModifierMathTests.cpp:180`, `FilterTests.cpp:194-202` — those encode the weaker requirement and must change with a constructor invariant.

### Council `config` pointers vs rebuild — **confirmed (latent; contract lie)**

- Header claims wrappers stay valid “across rebuilds” (`CouncilEffects.h:21-25`).
- `RebuildWorld` clears/refills `m_worldConfigs` then rebuilds wrappers (`CouncilEffects.cpp:20-36`, `:53-61`). Clearing a `std::vector` invalidates pointers previously handed out.
- `PlanetaryCouncil::CollectWorldEffects` returns **by value** (`PlanetaryCouncil.cpp:606-608`) — copies of wrappers whose `config` still point into `m_worldConfigs`. Today callers use the vector within the call (`GameState::CollectWorldEffects` `:136-137`; `ComputeVoteWeight` `:168-171`), so no live dangling. Package 1 may cache world/council contributions into a provider pool — that makes this latent bug real.
- `HasActiveRuleFlag` silently skips null `config` (`CouncilEffects.cpp:68-70`), contradicting “throw on unexpected null.”

### Lazy filters accept rvalues — **confirmed**

- Four filters take `const std::vector<ActiveEffect_t>&` and return borrowing views (`ActiveEffect.h:235-247`, `:254-267`, `:275-287`, `:297-303`), each documenting the borrow rule.
- Live prvalue call sites: `ResolveStat(UnitDesign)` / context overload pass `rDesign.CollectEffects()` straight into `FilterByStatId*` (`ActiveEffect.cpp:476`, `:482`). Same-full-expression today; range-for over that form would dangle without P2718R0.
- Workarounds already exist: `CollectFromPops` named locals (`:369-374`), `ResolveAdditiveStat` “Materialize first” (`:488-491`), unit `ResolveStat` locals (`:522`, `:528`).

### `HasPermission` vs `CollectLiveUnitEffects` — **confirmed (partial redundancy)**

- `CollectLiveUnitEffects` erases effects failing `UnitFilterSatisfied` (and ProducedAt mismatch) (`ActiveEffect.cpp:453-469`).
- `HasPermission` re-calls `UnitFilterSatisfied` (`:616-618`) on that already-filtered list — dead for the filter half.
- `HasPermission` still correctly re-checks `ConditionSatisfied` (`:620-623`): the collector does **not** evaluate conditions (they need `EffectContext_t`). `ResolveStat`/`ResolveFlag` already trust the collector’s unitFilter guarantee and do not re-check it.

### Hygiene in package scope

| Item | Status |
|------|--------|
| Null `config` skip in `CouncilEffects::HasActiveRuleFlag` | Confirmed (`CouncilEffects.cpp:68-70`) — throw instead |
| `Contribution` missing `_t` | Confirmed (`ActiveEffect.h:160`) — rename with call sites |
| Grant cycle parses `" -> "` from `sourceId` | Confirmed (`ActiveEffect.cpp:111-130`) — **defer structural fix to Package 2** (grant expand pipeline owns that function); do not couple this package to expand-order work |
| Comment “Contributions are applied in the order given” | Misleading vs `ApplyModifierStack` partition (`ActiveEffect.h:174`) — fix comment only |
| Pointer/`p` prefix on `config`/`originBase`/`targetTile` | Defer rename churn to Package 8 (touches every effects consumer) |

---

## Design decision

### Confirm with amends

| Area | Decision | Rejected alternatives |
|------|----------|----------------------|
| Origin tagging | Pass owning `BaseManager&` into `BuildingManager::CollectEffects`; forward to `AppendActiveEffects`. Export `constexpr bool TagsOriginBase(EffectScope_t)` next to `LaneFor`/`IsFactionLane` in `BonusEffect.h` and use it inside `AppendActiveEffectsIf_`. Delete the `BaseManager` re-tag loop. Same for `CollectFromPops` (append with origin; drop post-assign). | Export predicate but keep post-tag loop (still two sites). Keep post-tag but call `TagsOriginBase` only in BaseManager (buildings still pass null — architecture violation remains). |
| Instantaneous Infiltration | `DispatchInstantaneousEffects(..., GameState& rGameState)` — **no default, no null**. Remove stderr TODO branch; call `ApplyInfiltrationEffect` unconditionally for that variant. Wire production completion via a **Faction session back-pointer** (`GameState*` set in `GameState::AddFaction`, parallel to `SetSettings`/`BindWorldMap`). `BaseManager` reads `GetFaction().GetGameState()` and **throws** if null when dispatching (production in a live session must be bound). Direct test callers pass a real `GameState&`. | Inject `GameState&` into every `BaseManager` ctor (pulls session into a leaf that today’s fixtures construct without `GameState`). Keep `GameState*` with default `nullptr` (recreates the bug). Narrow “ledger-only” surface (insufficient: `ApplyInfiltrationEffect` needs factions + council membership). Reject Instantaneous Infiltration at parse time (wrong layer; Continuous already works; Instantaneous is a supported persistence). |
| `ActiveEffect_t` invariant | Constructor `ActiveEffect_t(const EffectConfig_t& rConfig, std::string sourceId, const BaseManager* pOriginBase = nullptr)` (plus `ownerFaction` defaulted unset). `config` is always non-null after construction. Delete aggregate null brace-init. Update `actest::Active` and remove “tolerate null config” tests (requirement change: null is not a valid instance). Hot-path null skips become unnecessary; prefer remove (or `assert`) rather than keep forever. | Document-only invariant (status quo). `std::reference_wrapper` / embed config by value (copies break identity with static registries; council already copies configs into its store). |
| Council storage | **Stable storage**: hold world/governor configs in `std::deque<EffectConfig_t>` (or `unique_ptr` nodes) so `ActiveEffect_t::config` addresses survive `RebuildWorld` / `SetGovernorEffects`. Keep wrappers rebuilt from those nodes. Update `CouncilEffects.h` comment to state the real guarantee: pointers remain valid for the lifetime of `CouncilEffects` (including across rebuilds). `HasActiveRuleFlag`: throw on null config. Optional same-PR: `PlanetaryCouncil::CollectWorldEffects` / `CollectFactionEffects` return `const std::vector<ActiveEffect_t>&` to avoid pointless copies (`PlanetaryCouncil.cpp:606-617`; full-review perf note) — do this if call sites allow. | “Valid until next rebuild; never retain” only — fragile once Package 1 caches world effects. |
| Lazy filters | `= delete` rvalue overloads for all four filters. Materialize in `ResolveStat(UnitDesign)` overloads (mirror `ResolveAdditiveStat`). Trim repeated borrow comments to one shared note. | Rely on P2718R0 / full-expression luck. |
| `HasPermission` | Document on `CollectLiveUnitEffects`: returned effects already satisfy `UnitFilterSatisfied` (and ProducedAt origin match). Drop the redundant `UnitFilterSatisfied` call inside `HasPermission`; keep `ConditionSatisfied`. | Re-check unitFilter in every resolve path (defeats collector). Drop condition check too (wrong). |

### Interactions with other packages

- **Package 1** (single pool): may retain council/world `ActiveEffect_t`s across turns — stable council storage here unblocks that safely. Coordinate if both touch `IEffectsProvider` / `CollectWorldEffects` return type.
- **Package 2** (grant expand): owns `processedGrantedIds` / cycle-guard redesign; leave `GrantChainContains_` alone here.
- **Package 4** (attribution): `ownerFaction` field stays; do not rename pointers in this package.
- **Package 8**: `p`-prefix renames, `Condition_t` variant redesign — out of scope.

---

## Implementation plan

1. **`TagsOriginBase`** in `BonusEffect.h` beside `LaneFor` / `IsFactionLane`:
   ```cpp
   constexpr bool TagsOriginBase(EffectScope_t scope)
   {
       const EffectLane_t lane = LaneFor(scope);
       return lane == EffectLane_t::Base
           || lane == EffectLane_t::ProducedAtBase
           || scope == EffectScope_t::FactionUnits;
   }
   ```
   Use it in `AppendActiveEffectsIf_`; delete the local boolean logic duplicate.

2. **`BuildingManager::CollectEffects(const BaseManager& rOriginBase)`** — pass `&rOriginBase` into `AppendActiveEffects`. Update header comment. `BaseManager::CollectBuildingEffects` becomes a one-liner forward (no re-tag loop).

3. **`CollectFromPops`** — append ThisBase effects with `&rOriginBase` (either filter-then-`AppendActiveEffects` on the matching configs, or append-all then keep FilterByScope but set origin at append). No post-loop assign.

4. **`ActiveEffect_t` constructor** — require `const EffectConfig_t&`; set `config = &rConfig`. Update `AppendActiveEffectsIf_`, `actest::Active`, grant-expansion clones, any `ActiveEffect_t{...}` sites. Remove null-config tests; replace with a compile/construct guarantee (and optionally one test that constructed effects always have non-null config).

5. **Templated `ResolveFlag` helper** — one range-based implementation shared by UnitDesign / Unit / Faction / BaseManager overloads (same pattern as `ResolveStatModifiers`).

6. **`DispatchInstantaneousEffects(..., GameState& rGameState)`** — remove default; infiltration always applies; GrantTech/GrantUnit stay TODO stderr (unimplemented).  
   - `Faction`: `BindGameState(GameState&)` / `GameState* GetGameState() const` (nullable when unbound).  
   - `GameState::AddFaction`: call `BindGameState(*this)`.  
   - `BaseManager` production lambda: `GameState* p = m_rFaction.GetGameState(); if (!p) throw ...; DispatchInstantaneousEffects(..., *p);`  
   - Update `BaseIntegrationTests` GrantBuilding case to construct/bind a `GameState` (or call Dispatch with a fixture `GameState`). Add an Instantaneous Infiltration production test that asserts ledger bits (multi-faction `GameState`).

7. **`CouncilEffects`** — `std::deque<EffectConfig_t>` (or equivalent) for `m_worldConfigs` / `m_governorConfigs`; rebuild wrappers from stable addresses; throw in `HasActiveRuleFlag` on null config; fix header contract text. Prefer return-by-const-ref for `WorldEffects` consumers that currently copy via `CollectWorldEffects()`.

8. **Filters** — deleted rvalue overloads; fix UnitDesign `ResolveStat` materialization; short shared borrow doc.

9. **`HasPermission`** — drop redundant unitFilter; document collector contract on `CollectLiveUnitEffects` declaration.

10. **Docs** — update `docs/architecture/effects-system.md`: Collection helpers (buildings pass origin); `ActiveEffect_t` non-null config; Instantaneous Infiltration requires session `GameState`; CouncilEffects stable storage; filter rvalue deleted; `CollectLiveUnitEffects` unitFilter guarantee.

11. **Hygiene in-scope** — rename `StatBreakdown_t::Contribution` → `Contribution_t`; fix ApplyModifierStack order comment. Do **not** rename `config`/`originBase` pointer fields (Package 8).

---

## Test plan

Requirements-based (update tests when the **requirement** changes; do not keep null-config pins).

1. **Origin tagging from buildings**  
   - Construct a building with `ThisBase` / `ProducedAtThisBase` / `FactionUnits` continuous effects; `CollectBuildingEffects` must set `originBase == &base` without any post-pass.  
   - Regression: `FilterForBase` / ProducedAt / FactionUnits home-base behaviour unchanged (existing BaseIntegration / UniversalRouting cases).

2. **`TagsOriginBase` tracks `LaneFor`**  
   - Static/unit test or Validation-style pin: every `EffectScope_t` enumerator — if `TagsOriginBase(s)` then append with non-null origin keeps it; FactionWide scopes leave origin null even when a base pointer was passed.

3. **Instantaneous Infiltration on building complete**  
   - Building fixture with Instantaneous Infiltration + factionFilter covering other council/AI factions.  
   - Complete production (or call `DispatchInstantaneousEffects` with `GameState&`).  
   - Require `DiplomacyLedger::HasInfiltration(beneficiary, target)` true for covered targets; not for self.  
   - Unbound faction (no `BindGameState`) + production dispatch → throws (not stderr).

4. **Instantaneous GrantBuilding still works** with a bound/`GameState&` (update existing `BaseIntegrationTests` case).

5. **`ActiveEffect_t` non-null**  
   - Remove “null configs ignored / filters tolerate null.”  
   - Construction goes through `actest::Active` / constructor only.

6. **Deleted rvalue filters**  
   - `FilterByStatId(std::vector<ActiveEffect_t>{}, …)` (or temporary return) is a **compile-time** failure — if hard to assert in Catch, rely on the deleted overload and materialize UnitDesign resolve paths; add a brief comment in FilterTests.

7. **`HasPermission`**  
   - Existing Amphibious / UniversalRouting permission cases still pass (condition path unchanged). No behavioural change expected from dropping redundant unitFilter.

8. **Council rebuild stability**  
   - Collect world effects vector; `RebuildWorld` with a different active set; **original** vector’s `config` pointers must still be readable **or** (if choosing return-by-ref + document non-retain) the test instead pins that a fresh collect after rebuild sees new configs and that deque addresses of *current* wrappers remain equal across a no-op rebuild. Prefer the stable-storage requirement: hold a copy of wrappers, rebuild adding another proposal, old wrapper `config->scope` still readable and unchanged.

---

## AI implementation prompt

```text
You are implementing Package 3 of the Alpha Centauri effects-model remediation
at /home/martok/alpha-centauri: Origin tagging & ActiveEffect API contracts.

Read first:
- docs/effects-fix-prompts/03-activeeffect-contracts.md (this package’s analysis — follow it)
- docs/effects-fix-packages.md § Package 3
- .devin/rules/coding-guidelines.md (references over pointers, throw on unexpected null,
  no legacy shims, constructors make objects valid)
- docs/architecture/effects-system.md (Collection helpers, ActiveEffect_t, Instantaneous)

## Goals

1. Single origin-tagging rule: `TagsOriginBase(scope)` next to `LaneFor` in BonusEffect.h;
   used only inside AppendActiveEffectsIf_. Buildings and pops pass the owning base into
   AppendActiveEffects. Delete BaseManager::CollectBuildingEffects re-tag loop and
   CollectFromPops post-assign.

2. DispatchInstantaneousEffects(building, base, GameState& rGameState) — no default.
   Instantaneous Infiltration always calls ApplyInfiltrationEffect. Wire production
   completion: Faction holds non-owning GameState* set by GameState::AddFaction
   (BindGameState); BaseManager throws if GetGameState() is null when dispatching.

3. ActiveEffect_t constructible only with const EffectConfig_t& (config always non-null).
   Update actest::Active and all hand-built sites. Remove null-config “tolerance” tests
   (requirement change). Collapse four ResolveFlag overloads to one templated helper.
   Drop redundant null skips that can no longer trigger (or assert).

4. CouncilEffects: stable config storage (std::deque<EffectConfig_t> or unique_ptr nodes)
   so ActiveEffect_t::config survives RebuildWorld/SetGovernorEffects. Fix header contract.
   HasActiveRuleFlag throws on null config. Prefer CollectWorldEffects/CollectFactionEffects
   return const ref if call sites allow.

5. Delete rvalue overloads for FilterByStatId / FilterByStatIdInContext /
   FilterBaseLevelByStatId / FilterByScope. Materialize before filter in
   ResolveStat(UnitDesign) overloads.

6. Document CollectLiveUnitEffects: unitFilter (and ProducedAt origin) already applied.
   HasPermission: remove redundant UnitFilterSatisfied; keep ConditionSatisfied.

7. Small hygiene: StatBreakdown_t::Contribution → Contribution_t; fix misleading
   “applied in the order given” comment on ApplyModifierStack. Do NOT rename
   config/originBase/targetTile pointer fields (Package 8). Do NOT rework
   GrantChainContains_ (Package 2).

8. Update docs/architecture/effects-system.md for the new contracts.

## Key files

- include/game/effects/BonusEffect.h, ActiveEffect.h
- src/game/effects/ActiveEffect.cpp
- include/game/faction/base/buildings/BuildingManager.h
- src/game/faction/base/buildings/BuildingManager.cpp
- src/game/faction/base/BaseManager.cpp
- include/game/Faction.h, src/game/Faction.cpp
- src/game/GameState.cpp (AddFaction → BindGameState)
- include/game/council/CouncilEffects.h, src/game/council/CouncilEffects.cpp
- include/game/council/PlanetaryCouncil.h, src/game/council/PlanetaryCouncil.cpp
- src/game/effects/InfiltrationRules.cpp/.h (consume only; don’t redesign)
- tests/TestHelpers.h, tests/effects/*, tests/GameFixtures.h as needed
- docs/architecture/effects-system.md

## Constraints

- Build/test only via ./bd (never raw cmake/make/ctest).
- No backwards-compat shims; update all call sites.
- Prefer references; Faction’s GameState* is the established optional-session pattern
  (like m_pSettings / m_pWorldMap).
- Unimplemented GrantTech/GrantUnit may keep TODO stderr; Infiltration must not.
- Do not fold world/council into IEffectsProvider (Package 1).
- Do not change grant expand order / processedGrantedIds (Package 2).
- Do not stamp unit-aura ownerFaction (Package 4).
- Do not tighten BonusEffectParser (Package 5).

## Acceptance criteria

- [ ] TagsOriginBase exists; BuildingManager tags via AppendActiveEffects; no BaseManager re-tag loop
- [ ] Production completion dispatches Infiltration into DiplomacyLedger when GameState bound
- [ ] Unbound GetGameState() during dispatch throws
- [ ] ActiveEffect_t cannot be built with null config; null-tolerance tests removed/replaced
- [ ] Rvalue filter overloads deleted; UnitDesign ResolveStat materializes
- [ ] Council config addresses stable across RebuildWorld (test pins this)
- [ ] HasPermission does not re-run UnitFilterSatisfied; collector docs state the guarantee
- [ ] ./bd test passes for effects / council / infiltration-related suites you touched
- [ ] effects-system.md updated

## Out of scope

Package 1 pool unification, Package 2 expand pipeline, Package 4 Detect/auras,
Package 5 parser strictness, Package 8 p-prefix / Condition variant redesign,
grant cycle string decoupling.
```
