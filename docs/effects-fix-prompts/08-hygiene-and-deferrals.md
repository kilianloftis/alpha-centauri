# Package 8 — Hygiene, type redesign, deferrals

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 8; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** **Confirm** the package split (quick wins now; condition/filter/selector variants as a solo late PR; architecture known-gaps track-only). **Amend** a few claims: `UnitFilterSatisfied` silent-false is structural, not a live shipped-config bug (parser already requires params); Manhattan→Chebyshev doc fix is also claimed by Package 6 — do it there if that lands first; several “hygiene” bullets are already owned by Packages 3–7 and must not be re-done here.

---

## Verified diagnosis

### 1. `Condition_t` / `UnitFilter_t` / `TileSelector_t` are kind-tagged bags of optionals — **confirmed**

| Type | Locus | Shape |
|------|-------|--------|
| `TileSelector_t` | `BonusEffect.h:154-158` | `kind` + `optional<string> improvement` |
| `Condition_t` | `BonusEffect.h:332-341` | `kind` + `value` + `values` + `conditions` (AllOf dual encoding) |
| `UnitFilter_t` | `BonusEffect.h:355-364` | `kind` + three optionals |

**AllOf dual encoding — confirmed.** Production uses both forms:

- Feature shorthand: `{"kind":"AllOf","values":["Rocky","Road"]}` (`config/improvements.json:209`, `abilities.json:83`)
- Nested conditions: `AllOf` + `conditions: [IsDefending, TargetTileHas Base, OriginBaseIsTargetBase]` (`config/buildings/buildings.json:206-211`)

`ConditionBodySatisfied_` evaluates both arms and ends with `!values.empty() || !conditions.empty()` (`ActiveEffect.cpp:258-272`) solely to reject an empty AllOf node.

**UnitFilter silent-false — confirmed structurally, amended for severity.** `UnitFilterSatisfied` returns false when the optional for the declared kind is absent (`ActiveEffect.cpp:298-306`). **However**, `ParseUnitFilter` / `ParseTileSelector` already throw if required params are missing (`BonusEffectParser.cpp:204-279`). Shipped JSON cannot reach the silent-drop path; hand-built / mutated structs can. The fix is still correct (invalid states should be unrepresentable) but this is not a live gameplay silent-no-op today.

**`EffectVariant_t` precedent — confirmed.** Same header already models sum types with `std::variant`; these three should match.

`FactionFilter_t` (`BonusEffect.h:378-381`) is a pure kind enum with no dead fields — **out of the variant redesign** (optional consistency only; not required).

### 2. `IsCouncilMemberTarget_` invents membership when no council — **confirmed**

```17:29:src/game/effects/InfiltrationRules.cpp
bool IsCouncilMemberTarget_(const GameState& rState, FactionId_t candidate)
{
    // ...
    if (const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil())
    {
        return pCouncil->IsCouncilMember(*pCandidate);
    }
    return pCandidate->GetDefinition().identity.participatesInCouncil;
}
```

- With council: explicit member list (`PlanetaryCouncil::IsCouncilMember`).
- Without: eligibility flag `identity.participatesInCouncil` — the same flag `CreatePlanetaryCouncil` uses only as a **constructor filter** (`GameState.cpp:383-388`, validated again at `PlanetaryCouncil.cpp:46`), not as membership.

**Latency nuance:** production `CouncilMembers` appears only on continuous governor Infiltration (`config/council/rules.json:12-15`). That path exists only after a council is created. Probe Infiltration uses `ActionTarget` (`config/probe_actions.json:16-20`). So the wrong branch is mostly **pre-council / test / mod** surface — still a second membership rule that the header comment advertises as intentional (`BonusEffect.h:374`: “or participatesInCouncil if none”). Fix the code **and** that comment.

**Chosen behaviour:** return `false` when there is no council (no council ⇒ no council members). Prefer fail-closed over throw: `HasInfiltration` / filter queries during early session setup should not abort.

### 3. `BonusEffect.h` vs `EffectEnums.h` name/content inversion — **confirmed**

- `BonusEffect.h` — no type named Bonus; holds `EffectScope_t`, `EffectLane_t`, `ModifierOp_t`, `ConditionKind_t`, `PermissionId_t`, `EffectConfig_t`, `EffectVariant_t`, …
- `EffectEnums.h` — only `StatId_t`, `SocialRatingId_t`, `RuleFlagId_t` (+ `KindFor` / `SeedFor`)

High include churn (dozens of `.h`/`.cpp`/tests). Naming pass is real but must be a **dedicated PR**, not mixed with correctness packages.

### 4. KindFor test gaps (`PsiDamage`, `TechCost`) — **confirmed**

`KindFor` already classifies both (`EffectEnums.h:107-108`, `:116` → Additive). `ValidationTests.cpp:56-87` claims to pin every stat’s kind but omits exactly those two. The pin’s purpose is closing the compile-time gap when a new `StatId_t` is added — the gap is open for the two missing asserts.

### 5. UnitDesign `CostMultiplier` empty-contributions workaround — **confirmed**

```119:124:src/game/units/UnitDesign.cpp
const StatBreakdown_t breakdown =
    ResolveStatModifiers(FilterByStatId(allEffects, StatId_t::CostMultiplier), SeedFor(StatId_t::CostMultiplier));
const float costMult = breakdown.contributions.empty() ? 1.0f : static_cast<float>(breakdown.total);
```

`SeedFor(CostMultiplier) == 1.0` (`EffectEnums.h:119-120`, pinned at `ValidationTests.cpp:93`). Empty contribution list → `ApplyModifierStack(1.0, {})` returns the seed. The ternary is dead and hides the documented seed rule.

### 6. Doc Manhattan vs code Chebyshev — **confirmed** (shared with Package 6)

`docs/architecture/effects-system.md:137` says Manhattan; `BonusEffect.h:402` and `ForEachTileInChebyshevRadius` say Chebyshev. Package 6’s prompt already includes this doc fix. **Package 8 only applies it if Package 6 has not landed yet.**

### 7. Architecture known-gaps — **confirmed; track only**

`docs/architecture/effects-system.md` Known Gaps (`:587+`) and `docs/code-review-findings.md:78-84` cover:

- Uncached / `(2r+1)²` area scans (needs spatial index + invalidation — not a variant of revision counters)
- Improvement faction-lane effects inert pending territory
- Other unfinished consumers (`NearZeroGrowth` / `PopulationBoom`, tile defense in combat, etc.)

These are **constraints on future work**, not Package 8 implementation items. Do not treat as missing features to “finish” here.

### 8. Hygiene owned elsewhere — **do not duplicate**

| Item | Owner |
|------|--------|
| `ActiveEffect_t` null `config` / constructor; `Contribution` → `Contribution_t`; pointer `p` prefix on `config`/`originBase`/`targetTile`; grant cycle `" -> "` coupling; `CollectActiveEffects` forwarder; misleading ApplyModifierStack order comment | Package 3 |
| `AppliesForFaction` / `ownerFaction` gate next to field | Package 4 |
| magic_enum for op/scope/persistence; `stod` hygiene; struct/parser balance defaults; `ValidateScopeForSource` | Package 5 |
| Chebyshev merge / max-radius / `bIsBaseTile`; preview API | Package 6 |
| Validator exhaustiveness; trailing `_` on free functions; validator doc drift | Package 7 |

Package 8 may still do **wire-map placement** for `StatId_t` / `RuleFlagId_t` / `SocialRatingId_t` (guidelines: one map next to the enum) **after** Package 5 decides magic_enum vs explicit maps for those snake_case ids — coordinate, do not fight Pass 1.

---

## Design decision

### Priority buckets

| Bucket | Items | PR shape |
|--------|-------|----------|
| **Quick wins** | Council membership fail-closed; KindFor pins; UnitDesign seed trust; Chebyshev doc if still wrong | One small PR anytime (even before 1–7 settle) |
| **Solo large PR (last)** | `Condition_t` / `UnitFilter_t` / `TileSelector_t` → `std::variant`; parser desugar; eval via `std::visit` | After Packages 1–5 stabilize (types stop moving) |
| **Solo medium PR** | Rename/split `BonusEffect.h` ↔ `EffectEnums.h`; optional wire-map relocation | After variants (or immediately after if variants deferred longer); never inside 1–5 |
| **Defer / track** | Known-gaps (area scan index, inert improvement faction-lanes, unused rule flags, …); hygiene owned by 3–7 | Architecture doc / backlog only |

### Chosen — Quick wins

1. **`IsCouncilMemberTarget_`:** if `GetPlanetaryCouncil()` is null → `return false`. Keep the IsCouncilMember path when present. Update `FactionFilterKind_t::CouncilMembers` comment (`BonusEffect.h:374`) to drop the participatesInCouncil fallback.
2. **KindFor pins:** add `static_assert(KindFor(StatId_t::PsiDamage) == StatKind_t::Additive)` and same for `TechCost` in `ValidationTests.cpp`.
3. **UnitDesign:** `costMult = static_cast<float>(breakdown.total)` — trust `SeedFor`.
4. **Doc:** Manhattan → Chebyshev in `effects-system.md` **only if** Package 6 has not already fixed it.

### Chosen — Variant redesign (large PR)

Represent sum types as variants (names illustrative):

```cpp
struct TargetTileHas_t { std::string featureId; };
struct AllOf_t { std::vector<Condition_t> conditions; }; // recursive
struct IsDefending_t {};
struct OriginBaseIsTargetBase_t {};
struct AttackerIsEmbarked_t {};
using Condition_t = std::variant<TargetTileHas_t, AllOf_t, IsDefending_t,
                                 OriginBaseIsTargetBase_t, AttackerIsEmbarked_t>;

struct UnitFilterDomain_t { UnitDomain_t domain; };
struct UnitFilterHasComponent_t { std::string component; };
struct UnitFilterHasFlag_t { RuleFlagId_t flag; };
using UnitFilter_t = std::variant<UnitFilterDomain_t, UnitFilterHasComponent_t, UnitFilterHasFlag_t>;

struct TileSelectorBaseTile_t {};
struct TileSelectorHasImprovement_t { std::string improvement; };
using TileSelector_t = std::variant<TileSelectorBaseTile_t, TileSelectorHasImprovement_t>;
```

**JSON wire form stays.** Parser desugars AllOf `"values": ["A","B"]` into `AllOf_t{ TargetTileHas(A), TargetTileHas(B) }` (and merges with nested `"conditions"` if both present). No mass config rewrite; one encoding after parse.

**Eval:** replace kind-switches with `std::visit` / exhaustive overload sets in `ConditionBodySatisfied_` / `UnitFilterSatisfied` / tile selector match. Malformed kind+missing-param states become unrepresentable — delete the optional-absent → false branches.

**Kinds enums:** delete `ConditionKind_t` / `UnitFilterKind_t` / `TileSelectorKind_t` once nothing references them (parser compares wire strings, then constructs alternatives).

### Chosen — Naming / file split (separate PR)

1. Move effect-domain enums (`EffectScope_t`, `EffectLane_t`, `ModifierOp_t`, `EffectPersistence_t`, permission/filter kinds if any remain, …) into `EffectEnums.h` **or** a new `EffectConfigEnums.h` if `EffectEnums.h` would grow too mixed with stats/flags.
2. Rename `BonusEffect.h` → `EffectConfig.h` (holds `EffectConfig_t`, `EffectVariant_t`, concrete effect structs). Update all includes; no compatibility typedef header.
3. Wire maps for snake_case `StatId_t` / `RuleFlagId_t` / `SocialRatingId_t`: relocate next to enums **after** Package 5’s parser work so maps are not moved twice.

### Rejected

| Alternative | Why not |
|-------------|---------|
| Throw from `IsCouncilMemberTarget_` when no council | Early-session queries should fail closed, not abort; false matches “no members” |
| Keep participatesInCouncil as intentional pre-council membership | Two rules; wider set; comment already wrong relative to PlanetaryCouncil membership |
| Rewrite production JSON to drop AllOf `values` | Unnecessary churn; parser desugar preserves one in-memory encoding |
| Fold variant redesign into Package 5 parser Pass 2 | Different goal (strictness vs type shape); mega-diff; wait until 1–5 settle |
| Implement `(2r+1)²` spatial index in Package 8 | Known-gap; needs its own design (invalidation on unit move vs improvement) |
| Rename BonusEffect.h inside the variant PR | Reviewable separately; variants already large |
| Re-fix Package 3–7 hygiene here | Duplicate / conflict |

### Package interactions

- **Schedule last** among remediation packages for the variant + rename work (`effects-fix-packages.md` dependency sketch).
- **Quick wins** may land anytime; they touch `InfiltrationRules.cpp`, `ValidationTests.cpp`, `UnitDesign.cpp`, optionally one doc line — low merge conflict risk with 1–7.
- **Package 6** owns Chebyshev doc if that PR is open; skip duplicate.
- **Package 5** may touch condition/filter/selector *parsing* (`.at()`, shape checks) — land Pass 1 before the variant PR so the variant PR ports already-strict parsers.
- **Package 3** may rewrite `ActiveEffect.h` heavily — variant PR depends on eval helpers in `ActiveEffect.cpp`; sequence after 3 if both edit condition evaluation.

---

## Implementation plan

### Phase F1 — Quick wins (one PR)

1. `InfiltrationRules.cpp`: no council → `false`; adjust any comment in `.h` if present.
2. `BonusEffect.h`: fix `CouncilMembers` comment (membership = PlanetaryCouncil member list only).
3. `ValidationTests.cpp`: add `PsiDamage` + `TechCost` KindFor asserts.
4. `UnitDesign.cpp`: drop empty-contributions ternary.
5. If `effects-system.md:137` still says Manhattan, change to Chebyshev (skip if Package 6 done).
6. Add a focused test: `FactionFilterCoversTarget` / `HasInfiltration` with `CouncilMembers` and **no** council ⇒ false for an eligible faction (construct GameState without `CreatePlanetaryCouncil`).
7. `./bd test` with filters covering infiltration / validation / unit design cost.

### Phase F2 — Variant redesign (solo large PR; after 1–5)

1. Introduce alternative structs + `using Condition_t = std::variant<…>` (same for filter/selector) in the effects header that owns them today.
2. Rewrite `ParseCondition` / `ParseUnitFilter` / `ParseTileSelector` to construct variants; desugar AllOf `values`; keep throwing on unknown kind / missing params.
3. Rewrite `ConditionBodySatisfied_`, `UnitFilterSatisfied`, `TileMatchesSelector_` (or equivalent) with exhaustive `std::visit`.
4. Update every hand-built `Condition_t` / `UnitFilter_t` / `TileSelector_t` in tests.
5. Delete unused `*Kind_t` enums.
6. Docs: brief note in `effects-system.md` that conditions/filters/selectors are variants (invalid combos unrepresentable); AllOf `values` is parse sugar.
7. Full effects + parser + combat-condition test pass via `./bd`.

### Phase F3 — Naming / file split (solo PR; after F2 preferred)

1. Rename/split headers; fix includes; update architecture docs that name `BonusEffect.h`.
2. Optionally relocate snake_case wire maps next to enums (post–Package 5).
3. No behaviour changes.

### Phase F4 — Deferrals (no code)

Leave Known Gaps entries as-is. When territory or combat fill-in lands, those packages must respect inert improvement faction-lanes and scan-cost constraints — do not “fix” by stubbing.

---

## Test plan

### F1 — Quick wins

| Requirement | Suggested case |
|-------------|----------------|
| No council ⇒ `CouncilMembers` matches nobody | GameState with ≥1 `participatesInCouncil` faction; no `CreatePlanetaryCouncil`; continuous Infiltration + `CouncilMembers` does **not** grant `HasInfiltration` / `FactionFilterCoversTarget` returns false |
| With council ⇒ membership list only | Existing governor Infiltration cases in `PlanetaryCouncilTests.cpp` stay green; alien with `participatesInCouncil=false` still excluded |
| KindFor pins cover PsiDamage + TechCost | `static_assert`s compile; both Additive |
| Design cost with no CostMultiplier effects uses seed 1.0 | Existing design-cost tests; optional assert `GetBaseCost()` equals raw component sum when no multipliers |

Do **not** weaken council membership tests to allow the eligibility fallback.

### F2 — Variants

| Requirement | Suggested case |
|-------------|----------------|
| AllOf `values` JSON still means AND of features | Existing Rocky+Road / Water+Base parser + runtime cases |
| AllOf nested conditions still mean AND | Creche-style IsDefending + TargetTileHas + OriginBaseIsTargetBase |
| Domain / HasComponent / HasFlag filters still gate units | Existing Aerospace / probe filter tests |
| HasImprovement / BaseTile selectors still match | Existing Farm booster / tile selector tests |
| Unknown kind / missing param still throw at parse | Existing ParserTests strictness (and Package 5 additions) |
| Empty AllOf rejected at parse | Already thrown today (`BonusEffectParser.cpp:171-173`); keep |

Requirement unchanged for gameplay; representation change only. Update tests that construct aggregates by field (`.kind = …`) to construct variant alternatives.

### F3 — Naming

Compile + existing suite; no new behavioural cases.

---

## AI implementation prompt(s)

### Prompt A — Quick wins (F1)

```text
You are implementing Package 8 Phase F1 (quick wins) of the effects-model remediation for the Alpha Centauri C++ rebuild at /home/martok/alpha-centauri.

## Goals

1. `IsCouncilMemberTarget_` / `FactionFilterKind_t::CouncilMembers`: when `GameState` has no PlanetaryCouncil, treat the candidate as **not** a council member (`return false`). Do not use `identity.participatesInCouncil` as a membership substitute.
2. Update the `CouncilMembers` comment in `BonusEffect.h` so it no longer documents the participatesInCouncil fallback.
3. Add KindFor `static_assert`s for `StatId_t::PsiDamage` and `StatId_t::TechCost` (both Additive) next to the other pins in `tests/effects/ValidationTests.cpp`.
4. In `UnitDesign::GetBaseCost`, use `breakdown.total` directly after `ResolveStatModifiers(..., SeedFor(CostMultiplier))` — remove the `contributions.empty() ? 1.0f : total` workaround.
5. If `docs/architecture/effects-system.md` still describes effect `radius` as Manhattan distance, change it to Chebyshev (code + `BonusEffect.h` already say Chebyshev). Skip if already fixed by Package 6.

Read for context: `docs/effects-fix-prompts/08-hygiene-and-deferrals.md` (F1 only), Package 8 in `docs/effects-fix-packages.md`, and the cited findings in `docs/effects-model-review.md` (`IsCouncilMemberTarget_`, KindFor gaps, UnitDesign workaround, Chebyshev doc).

## Constraints

- Follow `.cursor/rules/coding-guidelines.md` (throw over silent wrong defaults; no back-compat shims).
- Build/test only via `./bd`.
- Do **not** start the Condition/UnitFilter/TileSelector variant redesign.
- Do **not** rename BonusEffect.h / move enums.
- Do **not** implement Known Gaps (area-scan caching, territory collectors, etc.).
- Do **not** redo Package 3–7 hygiene (ActiveEffect constructor, AppliesForFaction, parser strictness, tile yield API, validators).

## Files to change (expected)

- `src/game/effects/InfiltrationRules.cpp`
- `include/game/effects/BonusEffect.h` (comment only)
- `tests/effects/ValidationTests.cpp`
- `src/game/units/UnitDesign.cpp`
- `docs/architecture/effects-system.md` (radius wording, only if still wrong)
- Tests: extend infiltration / council-filter coverage for the no-council case (prefer `tests/game/PlanetaryCouncilTests.cpp` or a small effects/infiltration test)

## Acceptance criteria

- No council ⇒ CouncilMembers filter matches no faction (including participatesInCouncil == true).
- Existing with-council governor Infiltration tests still pass.
- KindFor pins include PsiDamage and TechCost.
- GetBaseCost trusts SeedFor(CostMultiplier) with no empty-contributions special case.
- `./bd test` for the suites you touch passes; report failures and whether they are requirement changes vs bugs.

## What not to do

- Do not throw when council is null for this filter (fail closed with false).
- Do not change ActionTarget filter behaviour.
- Do not commit unless asked.
```

### Prompt B — Condition / UnitFilter / TileSelector variants (F2)

```text
You are implementing Package 8 Phase F2 (variant redesign) of the effects-model remediation for the Alpha Centauri C++ rebuild at /home/martok/alpha-centauri.

## Goals

Replace kind-tagged optional bags with `std::variant` sum types for:

- `Condition_t`
- `UnitFilter_t`
- `TileSelector_t`

so invalid kind/parameter combinations are unrepresentable. Keep JSON wire form stable via parser desugaring (especially AllOf `"values"` → nested `TargetTileHas` alternatives). Evaluate with exhaustive `std::visit` (or equivalent). Delete obsolete `ConditionKind_t` / `UnitFilterKind_t` / `TileSelectorKind_t` once unused.

Read first: `docs/effects-fix-prompts/08-hygiene-and-deferrals.md` (Design decision → Variant redesign, Test plan F2). Also Package 8 in `docs/effects-fix-packages.md` and the `[M] Condition_t / UnitFilter_t / TileSelector_t` finding in `docs/effects-model-review.md`.

## Prerequisites

- Prefer landing **after** Packages 1–5 (and Package 3 if it rewrites ActiveEffect condition paths). Port the **current** strict parsers; do not regress Package 5 throw-on-bad-shape behaviour.
- Do not combine with BonusEffect.h rename (Phase F3) unless rename is trivial mechanical follow-up in a second commit the user requested.

## Design (mandatory)

1. `Condition_t` alternatives at least: TargetTileHas (feature id), AllOf (vector<Condition_t>), IsDefending, OriginBaseIsTargetBase, AttackerIsEmbarked.
2. AllOf JSON may still supply `"values"` and/or `"conditions"`. After parse, only nested `Condition_t`s exist (desugar each values entry to TargetTileHas). Reject empty AllOf at parse (same requirement as today).
3. `UnitFilter_t`: Domain / HasComponent / HasFlag alternatives with required fields as plain members (not optionals).
4. `TileSelector_t`: BaseTile / HasImprovement alternatives.
5. `FactionFilter_t` stays as-is (pure kind enum) unless a tiny consistency change is free.
6. Runtime: `ConditionBodySatisfied_` / `UnitFilterSatisfied` / tile selector matching use visit; no “optional missing ⇒ false” paths.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md`.
- Build/test only via `./bd`.
- No legacy dual structs or `#ifdef` shims — update all call sites and tests.
- Do not hardcode balance numbers.
- Do not expand into parser Pass 2 dispatch-map refactor, pool rebuild, tile yield API, or Known Gaps.

## Files to change (expected)

- `include/game/effects/BonusEffect.h` (type definitions; or successor header if already renamed)
- `src/game/effects/BonusEffectParser.cpp` / `.h`
- `src/game/effects/ActiveEffect.cpp` (condition + unit filter eval)
- `src/game/effects/TileEffectsContext.cpp` (selector match)
- All tests/fixtures that construct these types in C++
- `docs/architecture/effects-system.md` — short note on variant encoding + AllOf values sugar

## Acceptance criteria

- All existing condition / unitFilter / selector gameplay behaviours preserved (Creche AllOf, Farm selectors, Domain filters, probe HasFlag filters, etc.).
- Parser still throws on unknown kinds and missing required params.
- No `*Kind_t` enums left for these three sum types.
- `./bd test` for effects/parser/combat/morale/tile suites you touch passes; report any requirement-change test updates explicitly.

## What not to do

- Do not mass-edit production JSON to remove AllOf `values` (desugar in parser).
- Do not rename BonusEffect.h ↔ EffectEnums.h in this PR (Phase F3).
- Do not change IsCouncilMemberTarget_ / KindFor pins / UnitDesign (should already be done in F1).
- Do not commit unless asked.
```

### Prompt C — Header naming / enum placement (F3)

```text
You are implementing Package 8 Phase F3 (naming / header split) of the effects-model remediation for the Alpha Centauri C++ rebuild at /home/martok/alpha-centauri.

## Goals

1. End the BonusEffect.h vs EffectEnums.h name/content inversion:
   - Effect-domain enums live with other effect enums (extend `EffectEnums.h` or add `EffectConfigEnums.h` if clearer).
   - Rename `BonusEffect.h` → `EffectConfig.h` (or equally accurate name) for `EffectConfig_t` / `EffectVariant_t` / concrete effect structs.
2. Update all includes and architecture docs that cite the old filename.
3. Optionally relocate snake_case wire maps for `StatId_t` / `RuleFlagId_t` / `SocialRatingId_t` next to those enums per coding guidelines — **only if** Package 5 has finished deciding magic_enum vs explicit maps for those ids (do not fight an in-flight parser PR).

Read: `docs/effects-fix-prompts/08-hygiene-and-deferrals.md` (F3). Prefer doing this **after** Phase F2 variants so types are stable.

## Constraints

- Mechanical rename/move only — no behaviour changes.
- No compatibility shim headers that re-export the old name.
- Build/test only via `./bd`.
- Do not implement Known Gaps or revisit variant design.

## Acceptance criteria

- No includes of `BonusEffect.h` remain (unless intentionally kept as a one-line `#error` redirect — prefer none).
- Project compiles; full `./bd test` (or effects-focused + any broken include sites) green.
- Architecture docs name the new header(s).

## What not to do

- Do not mix in ActiveEffect pointer renames / Contribution_t (Package 3) unless already done and you are only fixing includes.
- Do not commit unless asked.
```

### Prompt D — Deferrals (documentation only; optional)

```text
No implementation. When touching territory ownership collectors or TileEffectsContext performance, re-read Known Gaps in docs/architecture/effects-system.md and docs/code-review-findings.md:78-84. Do not stub improvement faction-lane application or a fake area-scan cache from Package 8 hygiene work.
```
