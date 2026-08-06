# Effects model — work packages

**Date:** 2026-08-04  
**Source of findings:** [`docs/effects-model-review.md`](effects-model-review.md)  
**Goal:** Group related review findings into eight attack packages. Each package below is self-contained enough for a dedicated analysis pass that produces an AI implementation prompt.

**Project constraints (always apply):**
- Follow `.cursor/rules/coding-guidelines.md` (SOLID, references over pointers, throw over silent defaults, no legacy/back-compat shims).
- Build/test only via `./bd` (never raw cmake/make/ctest).
- Prefer config/Lua for moddability; do not hardcode game numbers in C++.
- Unimplemented features are fine; silent wrong values and expensive-to-unwind stubs are not.
- Keep architecture docs under `docs/architecture/` current when boundaries change (`effects-system.md` especially).

**Suggested sequence:** A → B → C/D → E → F/G → H (packages 1→8). Do not merge package 5’s dispatch-table refactor with its strictness fixes in the same change unless analysis says otherwise.

---

## Package 1 — Single source of truth for base effects (Phase A)

**Priority:** Highest. Live player-visible wrong answers.  
**Theme:** Every consumer of “this base’s effects” / resolved stats must see one pool and one rounding rule.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [H] | Two divergent effect lists per base: memo omits world and council effects | `BaseManager::BuildBaseEffects_` vs `ProduceResources`/`ApplyGrowth` caller-supplied pool |
| [H] | Base-level percentage modifiers are silently discarded | `ResourceManager.cpp` seeds `ResolveStatModifiers` with `SeedFor(stat)` (0) then *adds* to worked total |
| [H] | Two different int-rounding policies for the same stat | `ResolveStat` truncates; combat/`PlanetaryCouncil` use `lround` |

### Problem statement

Today three independent bugs compound:

1. **Two pools.** Turn stages that collect resources/growth append `GameState::CollectWorldEffects()` (other factions’ `WorldGlobal`, council laws, governor faction-globals) onto a fresh base list. Memoized paths (`ApplyProduction`, mineral cost, production getters, `GetNutrientsRequired`, tile yields for UI) use only `IEffectsProvider` → `FactionEffectsPool`, which never includes those world/council contributions. Same base, same turn: ResourceCollection banks minerals *with* council effects; BaseProduction charges costs *without*; GrowthDisplay can disagree with `ApplyGrowth`.

2. **Wrong seed for base-level %.** Selector-free base modifiers (e.g. Economy rating minerals −10%/−20% in `config/social_rating_effects.json`) are applied as `worked + ResolveStatModifiers(filter, SeedFor(stat)).total`. With seed 0, `AddPercent` vanishes inside `ApplyModifierStack`. Tile path deliberately only forwards *selector-carrying* modifiers, so those penalties apply nowhere.

3. **Rounding fork.** `Unit::GetStat` / movement go through `ResolveStat` → `static_cast<int>`; combat morale and council vote weight `lround` the same stack. Chassis `+25%` attack on strength 2 yields 2 vs 3 depending on path.

### Likely fix direction (from review; validate in analysis)

- Fold world/council contributions into the provider’s pool (or a single session-scoped aggregator) so `BuildBaseEffects_()` is the only entry; delete `FactionEffects_t`-taking overloads that invite divergence.
- Seed base-level resolve with the worked value (mirror `Pop::ApplyTileMultipliers`).
- Centralize float→int in the `ResolveStat` family; make combat/council call it (pick one convention and document it next to `ApplyModifierStack` / `SeedFor`).

### Key files

- `include/game/IEffectsProvider.h`, `src/game/faction/FactionEffectsPool.cpp`, `include/game/faction/FactionEffectsPool.h`
- `src/game/faction/base/BaseManager.cpp`, `include/game/faction/base/BaseManager.h`
- `src/game/Faction.cpp` (`ProduceBaseResources`, `ApplyBaseGrowth`)
- `src/game/GameState.cpp` (`CollectWorldEffects`)
- `src/game/faction/base/resources/ResourceManager.cpp`
- `src/game/effects/ActiveEffect.cpp` / `.h` (`ResolveStat`, `ApplyModifierStack`, `SeedFor`)
- `src/game/units/MoraleCalculator.cpp`, `src/game/council/PlanetaryCouncil.cpp`
- `src/game/stages/BaseProduction.cpp`, ResourceCollection / Population stages
- `config/social_rating_effects.json`, unit chassis/specials for rounding fixtures
- Tests: effects modifier math, base production/resources, any council vote weight tests

### Risks / invariants

- Invalidation: memo key is pool version; world/council must bump something the memo sees when laws change.
- Do not break `FactionEffects_t` / `BaseEffects_t` lane typing.
- Changing rounding changes combat and movement numbers — need explicit golden tests.

### Out of scope for this package

Pool *internal* expand order (package 2), parser shape checks (package 5), tile preview API (package 6).

### Analysis output path

`docs/effects-fix-prompts/01-single-source-of-truth.md`

---

## Package 2 — FactionEffectsPool rebuild / expand pipeline (Phase B)

**Priority:** High. Mostly latent until grants/rating mods land on pops/units; shapes the model’s expansion contracts.  
**Theme:** One ordered pipeline: collect → gate → expand → stamp, with lane-correct rating accumulation.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [M] | Social-rating expansion runs before pop and unit effects are collected | `FactionEffectsPool::Rebuild_` |
| [M] | `removed_by_tech` filtered after expansions | same |
| [H] | Do not accumulate `ThisBase` modifiers for FactionUnits expansion | `SocialRatingResolver::ExpandFactionLaneSocialRatingEffects` |
| [H] | Grant expansion double-counts a building the base already constructed | `ExpandGrantBuildingEffects` / `processedGrantedIds` |
| [M] | Memoized pool not bound to the faction it was built for | `FactionEffectsPool::Get` |
| [M] | Rebuild stamp re-collected after rebuild | `Validate_` / `Rebuild_` |
| [H] | Research cost cache keyed on effects version but also tech count | `ResearchManager` (small, independent; include here or note as optional add-on) |

### Problem statement

`Rebuild_` interleaves collection and expansion incorrectly:

- Rating expansion sees buildings/policies but not yet pop/unit faction-lane modifiers → base lane and faction lane can disagree on the same axis.
- `removed_by_tech` runs last, so grant/rating *derivatives* survive after the gate effect is stripped.
- Faction-lane rating expansion accumulates the raw faction pool including every base’s `ThisBase` `SocialRatingModifier`s, then expands `FactionUnits` gameplay effects from that inflated total.
- Grant dedupe only tracks grant-vs-grant, not grant-vs-already-constructed (Command Nexus / Perimeter Defense case).
- Cache identity and stamp timing are footguns for future collectors.

### Likely fix direction

Canonical order sketch (confirm against docs):

1. Collect all raw continuous contributors (defs, buildings, policies, pops, units, tile-yield rules, …).
2. Apply `removed_by_tech` (and similar gates) **before** any expansion.
3. Expand grants with `processedGrantedIds` pre-seeded from constructed `{originBase, sourceId}`.
4. Expand social ratings: faction-lane accumulation only over FactionWide-lane modifiers; base-lane after `FilterForBase`.
5. Stamp cache from the revision vector read **before** rebuild; bind pool to owning `Faction&` in the constructor.
6. Research cost: also key on `ResearchManager`’s own revision / tech count.

### Key files

- `src/game/faction/FactionEffectsPool.cpp`, `include/game/faction/FactionEffectsPool.h`
- `src/game/effects/ActiveEffect.cpp` (grant expansion)
- `src/game/social-engineering/SocialRatingResolver.cpp`, `.h`
- `src/game/faction/base/BaseManager.cpp` (`BuildBaseEffects_` rating expand)
- `src/game/faction/ResearchManager.cpp`
- `tests/effects/GrantExpansionTests.cpp`, social rating / pool tests
- `docs/architecture/effects-system.md`

### Risks / invariants

- Revision graph must still invalidate on every contributor mutator (already strong; don’t regress).
- Lane types (`FactionEffects_t` / `BaseEffects_t`) must keep compile-time mistakes impossible.
- Fixtures already use `ThisBase` Growth — regression test multi-base morale/probe.

### Out of scope

World/council folding into provider (package 1) — but coordinate if both touch `IEffectsProvider`.

### Analysis output path

`docs/effects-fix-prompts/02-pool-rebuild-pipeline.md`

---

## Package 3 — Origin tagging & ActiveEffect API contracts (Phase C)

**Priority:** Medium-high. Prevents misuse and completes Instantaneous Infiltration.  
**Theme:** `ActiveEffect_t` construction and dispatch invariants are structural, not conventional.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [M] | Origin tagging has a second, hand-maintained implementation | `ActiveEffect` + `BaseManager::CollectBuildingEffects` |
| [M] | `CollectBuildingEffects` re-implements origin-tagging (cross-slice duplicate of above) | same |
| [M] | `DispatchInstantaneousEffects` optional `GameState` drops Infiltration | `ActiveEffect.h` + `BaseManager` completion path |
| [M] | Instantaneous Infiltration unreachable from production completion (duplicate) | `BaseManager.cpp:113` |
| [M] | Four copies of flag scan + null config checks nothing can trigger | `ResolveFlag` / `AppendActiveEffectsIf_` |
| [M] | Council `ActiveEffect_t::config` pointers outlive guarantee after rebuild | `CouncilEffects` |
| [M] | Lazy filters accept rvalue containers | filter helpers in `ActiveEffect.h` |
| [M] | `HasPermission` re-filters what collector already removed | `ActiveEffect.cpp` |
| — | Hygiene: null `config` skip in `CouncilEffects`; grant cycle string coupling; `Contribution` naming | various |

### Problem statement

- Origin base tagging is derived from `LaneFor` in `AppendActiveEffects`, then re-done with a hardcoded scope triple in `BaseManager` because buildings pass `nullptr` origin.
- Instantaneous `Infiltration` on building complete always hits the TODO/stderr path because `GameState*` defaults to null and the only caller omits it.
- `ActiveEffect_t::config` is a raw pointer with no constructor invariant; consumers null-check forever; council rebuilds can dangle retained wrappers.
- Filter views borrow vectors but accept temporaries; permission path disagrees with collector guarantees.

### Likely fix direction

- Pass owning base into `BuildingManager::CollectEffects` → `AppendActiveEffects`; export `TagsOriginBase` if needed; delete re-tag loop.
- `DispatchInstantaneousEffects(..., GameState&)` (or narrower infiltration surface); wire from base completion.
- Construct `ActiveEffect_t` only with `const EffectConfig_t&`; stable storage for council configs (deque/`unique_ptr`) **or** document “valid until next rebuild; never retain” and return by value only within call.
- `= delete` rvalue filter overloads; one templated `ResolveFlag`; pick collector vs `HasPermission` contract and document.

### Key files

- `include/game/effects/ActiveEffect.h`, `src/game/effects/ActiveEffect.cpp`
- `src/game/faction/base/buildings/BuildingManager.cpp`, `BaseManager.cpp`
- `src/game/council/CouncilEffects.cpp`, `.h`
- `src/game/effects/InfiltrationRules.cpp`
- Tests that build `ActiveEffect_t` by hand (`modifierMathTests`, etc.)

### Risks

- Injecting `GameState` into `BaseManager` completion may pull session wiring — prefer narrow interface if possible.
- Council performance callers that copy world effects each frame (left in full review) may interact with storage choice.

### Analysis output path

`docs/effects-fix-prompts/03-activeeffect-contracts.md`

---

## Package 4 — Attribution, auras, Detect (Phase C)

**Priority:** Medium. Latent until unit-component Detect/Conceal ships; wrong-by-default for mods.  
**Theme:** Every projected tile effect has an owning faction; visibility trusts that.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [M] | Unit-projected auras carry no faction attribution | `TileEffectsContext::CollectAreaEffects` |
| [M] | `Detect` with no `ownerFaction` reveals to every faction | `UnitVisibility` / `AppliesForFaction_` |
| [M] | `IsUnitVisibleTo` re-collects tile area effects per concealment channel | `UnitVisibility.cpp` |
| — | Deduplicate `AppliesForFaction_` (TileEffectsContext vs UnitVisibility) | both |
| — | Hygiene: `ownerFaction` gate belongs next to the field | `ActiveEffect.h` |

### Problem statement

Improvement auras stamp `ownerFaction` when territory-owned; unit auras leave it unset → “applies to all observers.” `Detect`/`Conceal` on units would pierce or hide for everyone. Visibility also rescans the neighbourhood once per concealment channel.

### Likely fix direction

- Stamp `ownerFaction` from `pUnit->GetFaction()` for unit-projected effects.
- Single shared `AppliesForFaction` next to `ownerFaction`; require owner on Detect (or always stamp).
- Collect area effects once in `IsUnitVisibleTo` and pass down.

### Key files

- `src/game/effects/TileEffectsContext.cpp`, `.h`
- `src/game/faction/UnitVisibility.cpp`, `.h`
- `config/unit_components` (Carrier_Deck ThisTile flag), Sensor Detect
- Visibility / probe tests

### Analysis output path

`docs/effects-fix-prompts/04-attribution-auras-detect.md`

---

## Package 5 — Parser strictness & honored shapes (Phase D)

**Priority:** High for modder fail-loud; dispatch refactor is large follow-on.  
**Theme:** Illegal or unsupported effect JSON fails at load, never silently no-ops at runtime.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [H] | Read required keys with `at()`, not `operator[]` | `BonusEffectParser` |
| [H] | `ParseEffectConfig` 365-line if/else of all invariants | same |
| [M] | Game-balance numbers as silent parser defaults | same (+ struct defaults in `BonusEffect.h`) |
| [M] | Effect structs carry balance defaults the parser also carries | `BonusEffect.h` |
| [M] | `EffectiveStatModifierAmount` 0.0 for missing context / non-Add ops | parser + resolve |
| [H] | Proposal effects accepted in shapes council never applies | `CouncilProposalConfigParser` |
| [M] | `governor_effects` without shape checks | `CouncilRulesConfigParser` |
| — | `ValidateScopeForSource` too weak (`ThisBase` on unit components) | `BonusEffectParser` |
| [M] | `WorldGlobal` `SocialRatingModifier` accepted but inert (**owed to package 2**) | `BonusEffectParser` |
| — | Tile yield rules skip validating `ParseEffects` overload | `TileYieldRulesConfigParser` |
| — | magic_enum for op/scope/persistence wire maps; `is_array` on effects; stod hygiene | parser |

### Problem statement

Two layers of silence: (1) parser accepts missing type/scope (abort/UB), omitted balance keys (invent SMAC numbers), and inert radius/scopes; (2) council/governor parsers accept effect shapes the runtime drops. Struct member defaults duplicate parser defaults.

### Likely fix direction

**Pass 1 — strictness (do first):** `.at()`; require balance keys or throw; drop conflicting struct defaults; restrict `amount_source`; council/governor honored-shape validators; tighten `ValidateScopeForSource`; validate tile-yield effects.

Package 2 owes one specific rule to this pass: it resolved the rating lane split by making
social ratings a faction-*local* axis (both lanes accumulate over
`IEffectsProvider::GetLocalActiveEffects`), so a `SocialRatingModifier` declared at
`WorldGlobal` — or reaching a faction as a council extra — now moves no rating on either
lane. `ValidateScopeForSource` must reject `WorldGlobal` on `SocialRatingModifier` (and the
council proposal/governor effect validators must reject it too) so the restriction fails
loud at load instead of silently doing nothing. Comments in `SocialRatingResolver.h`,
`BaseManager::GetEffectiveSocialRating` and `docs/architecture/effects-system.md` point
here; update all three when it lands.

**Pass 2 — structure:** per-type parse functions + dispatch map; shared `RequireScope_` / resource-stat helpers. Do not conflate with Pass 1 in one mega-diff unless necessary.

### Key files

- `src/game/effects/BonusEffectParser.cpp`, `.h`
- `include/game/effects/BonusEffect.h`
- `src/game/council/CouncilProposalConfigParser.cpp`, `CouncilRulesConfigParser.cpp`
- `src/game/effects/TileYieldRulesConfigParser.cpp`
- `tests/effects/ParserTests.cpp`, council config tests
- `docs/architecture/effects-system.md`, `council-system.md`

### Analysis output path

`docs/effects-fix-prompts/05-parser-strictness-shapes.md`

---

## Package 6 — Tile yield resolution API (Phase E)

**Priority:** Medium. UI lies about unworked tiles; hot path copies.  
**Theme:** Preview means “as if worked”; resolve shouldn’t deep-copy three times per tile for totals.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [M] | Preview yield vs worked yield disagree on selector modifiers | `TileEffectsContext` |
| [M] | `ResolveYieldFromEffects_` copies effect list three times; sorts discarded breakdowns | same |
| — | Shared Chebyshev traversal for CollectAreaEffects; max-radius rule consistency | same |
| — | Known gap: `(2r+1)²` area scans (defer unless free) | architecture / prior findings |
| — | Observed: WorkerAssignment sort comparator re-resolves yields | optional follow-up |
| — | Observed: `ResolveStatModifiers` always materializes sorted breakdown | optional; may touch package 1 |

### Problem statement

`ResolvePreviewTileYield` skips `AppendMatchingTileModifiers_` while the base screen uses preview for unworked tiles — so “+1 nutrient to worked Farms” shows the wrong number for placement decisions. Tests currently pin the discrepancy. Hot path deep-copies `ActiveEffect_t` vectors and builds unused contribution sorts.

### Likely fix direction

- Unify overloads; preview = as-if-worked (include selector pass); update tests that pinned the old behavior as a **requirement change**.
- In-place suppress filter; total-only resolve path without sort.
- Optional: one neighbourhood walk for collect; don’t expand known-gap radius caching unless scoped in.

### Key files

- `src/game/effects/TileEffectsContext.cpp`, `.h`
- `src/ui/base/BaseWorkableAreaDisplay.cpp`
- `tests/game/TileResourceRestrictionTests.cpp`
- `docs/architecture/effects-system.md` (radius distance: Chebyshev vs doc Manhattan)

### Analysis output path

`docs/effects-fix-prompts/06-tile-yield-api.md`

---

## Package 7 — Load-time effect reference validation (Phase E)

**Priority:** Medium. Prevents typo’d ids becoming silent no-op effects.  
**Theme:** Validators are exhaustive, fail on missing registries, cover every effects source.

### Findings included

| Sev | Title | Primary locus |
|-----|-------|-----------------|
| [M] | Effect validator variant dispatch has no exhaustiveness guard | `EffectReferenceValidator` |
| [M] | Missing registry silently disables validation | `EffectReferenceValidator` + `RequiredTechValidator` |
| — | Probe-action effects never id-checked | `EffectReferenceValidator` walk list |
| — | Doc comment drift; trailing `_` on free functions | hygiene |

### Problem statement

`if/else if` + `get_if` over `EffectVariant_t` won’t fail compile when a new id-bearing alternative is added. Null registries no-op the whole check. Probe actions omitted from the walk. RequiredTech validator shares the “null = skip” and “hand-maintained registry list” problems.

### Likely fix direction

- `std::visit` overload set covering every alternative (`-Werror` unused / exhaustive patterns as available).
- `GameDataContext` overloads take registries by reference; keep nullable only on narrow test overloads.
- Add `probeActionsConfig` to the walk; consider generating or sharing the registry list with `LoadGameData`.

### Key files

- `src/game/EffectReferenceValidator.cpp`, `.h`
- `src/game/RequiredTechValidator.cpp`, `.h`
- `src/game/GameDataContext.cpp` / load path
- `tests/game/*Validator*`

### Analysis output path

`docs/effects-fix-prompts/07-effect-reference-validation.md`

---

## Package 8 — Hygiene, type redesign, deferrals (Phase F)

**Priority:** Lowest / opportunistic. High churn or non-blocking.  
**Theme:** Clarity and type safety without blocking correctness packages.

### Findings included

| Sev | Title | Notes |
|-----|-------|-------|
| [M] | `Condition_t` / `UnitFilter_t` / `TileSelector_t` as kind + optionals | Large: move to `std::variant`; schedule alone |
| [M] | `IsCouncilMemberTarget_` invents membership without council | Small, isolated |
| [L] | `BonusEffect.h` vs `EffectEnums.h` name/content inversion | Rename/split |
| [L] | Pointer/`_t`/comment hygiene in ActiveEffect & parsers | Batch |
| — | Wire maps next to enums; KindFor test gaps (`PsiDamage`, `TechCost`) | |
| — | UnitDesign CostMultiplier empty-contributions workaround | |
| — | Architecture known-gaps (uncached scans, inert improvement faction-lanes) | Track only unless tackling |

### Likely fix direction

- Separate PR for condition/filter/selector variants after packages 1–5 stabilize.
- Quick wins: council membership rule; KindFor test completeness; doc Chebyshev; naming passes.

### Key files

- `include/game/effects/BonusEffect.h`, `EffectEnums.h`
- `src/game/effects/ActiveEffect.cpp` (condition evaluation)
- `src/game/effects/InfiltrationRules.cpp`
- `docs/architecture/effects-system.md`
- `tests/effects/ValidationTests.cpp`

### Analysis output path

`docs/effects-fix-prompts/08-hygiene-and-deferrals.md`

---

## Cross-package dependency sketch

```text
[1 Single pool + % + rounding] ──┬──► [2 Pool expand order]
                                 └──► [6 Tile yield] (uses Resolve paths)
[3 ActiveEffect contracts] ◄── coordinates with [1] on provider/session wiring
[4 Attribution] ── independent; can parallel [3]
[5 Parser Pass1 strictness] ── independent; Pass2 after Pass1
[7 Validators] ── after or with [5] (new types must be visited)
[8 Hygiene / variants] ── last
```

## Per-package analysis agent instructions (common)

Each analysis agent should:

1. Read this package section and the cited findings in `docs/effects-model-review.md` in full.
2. Read the key source/header files and relevant tests/config; verify claims with `path:line`.
3. Note interactions with other packages (blockers, merge conflicts, shared types).
4. Decide: confirm review fix, amend it, or split further — with rationale.
5. Write **only** the output file for that package, containing:
   - Short verified diagnosis
   - Chosen design (and rejected alternatives)
   - Test plan (requirement-based)
   - A ready-to-paste **AI implementation prompt** (self-contained: goals, files, constraints, acceptance criteria, what not to do)
6. Read-only otherwise. No builds unless essential; prefer not to run `./bd` during analysis.
