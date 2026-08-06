# Package 7 — Load-time effect reference validation

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 7; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** **Confirm** the review fix direction, with two amendments (below).

---

## Verified diagnosis

### 1. Variant dispatch has no exhaustiveness guard — **confirmed**

`ValidateEffectReferences` (list overload) uses an `if/else if` chain of `std::get_if` over three alternatives only:

```52:74:src/game/EffectReferenceValidator.cpp
        if (const auto* pGrant = std::get_if<GrantBuildingEffect_t>(&rEffect.effect))
        {
            if (pBuildings && !pBuildings->Find(pGrant->buildingId))
            {
                ThrowBadReference_(rSourceId, "building", pGrant->buildingId);
            }
        }
        else if (const auto* pTech = std::get_if<GrantTechEffect_t>(&rEffect.effect))
        {
            // ...
        }
        else if (const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect))
        {
            // selector improvement check
        }
```

`EffectVariant_t` has **18** alternatives (`include/game/effects/BonusEffect.h:291-310`). A new id-bearing alternative compiles cleanly and is never checked — the exact silent-no-op failure mode this validator exists to prevent.

Contrast: `KindFor` (`include/game/effects/EffectEnums.h:92-128`) is an exhaustive `switch` enforced by `-Werror=switch` (`src/CMakeLists.txt:138`). No equivalent exists for `EffectVariant_t`.

Condition feature ids (`:84-117`) and `unitFilter` component ids (`:119-125`) live on `EffectConfig_t` outside the variant — those paths are fine; they are not the exhaustiveness gap.

`GrantUnitEffect_t` is deliberately unvalidated (header comment at `include/game/EffectReferenceValidator.h:20-21`) — any exhaustive visitor must keep an **explicit** no-op arm for it, not a generic catch-all.

### 2. Missing registry silently disables validation — **confirmed**

**RequiredTech** early-returns when `techRegistry` is null:

```46:49:src/game/RequiredTechValidator.cpp
    if (!rData.techRegistry)
    {
        return;
    }
```

**EffectReference** `GameDataContext` overload passes raw `.get()` pointers (`src/game/EffectReferenceValidator.cpp:131-134`); each id check is gated on `if (pRegistry && …)`, so a null target registry skips that family of checks. Source walks are also `if (rData.X)` blocks (`:142-204`) — a null effect-source registry means that source is never walked.

`LoadGameData` always populates these registries before calling both validators (`src/game/GameDataContext.cpp:45-120`), so the nullable path exists only for partial test fixtures. Consequence of a wrong null on the production path: every cross-config id check passes vacuously.

Tests currently **encode silent skip as the requirement**:

- `tests/game/RequiredTechValidatorTests.cpp:85-89` — empty `GameDataContext` → `CHECK_NOTHROW`
- `tests/effects/ValidationTests.cpp:107-108` and `:189-190` — null registry on the **list** overload → skip (keep this; partial validation is intentional for unit tests)

### 3. `probeActionsConfig` omitted from the effect walk — **confirmed**

Walk list ends at council rules + `tileYieldRules` (`src/game/EffectReferenceValidator.cpp:142-205`). `rData.probeActionsConfig` is never visited, despite:

- being loaded in `LoadGameData` (`src/game/GameDataContext.cpp:102-104`) before validation
- carrying `std::vector<EffectConfig_t> effects` per action (`include/game/units/ProbeActionConfig.h:101-102`)
- stock config already shipping effects (`config/probe_actions.json:14-21`, Infiltration on infiltrate)

Probe-action effect id typos therefore never fail at startup.

### 4. RequiredTechValidator overlap — **confirmed**, with amendment

Same structural problems:

| Issue | EffectReference | RequiredTech |
|-------|-----------------|--------------|
| Null target registry = no-op | `.get()` + `if (p)` | early return `:46-49` |
| Hand-maintained source list | `:142-205` | `:52-79` |
| Doc comment drift | header omits factions/council/tile-yield (`include/game/EffectReferenceValidator.h:29-31`; code walks them `:187-205`) | header omits council proposals (`include/game/RequiredTechValidator.h:8-9`; code walks them `:76-79`) |
| Trailing `_` on free functions | `ThrowBadReference_` `:34` | `ValidateRequiredTech_` `:28` |

**Amendment A — also walk probe `requiredTech`:** `ProbeActionConfig_t` has `requiredTech` (`include/game/units/ProbeActionConfig.h:97`), and stock config sets it (`config/probe_actions.json:89`, `gene_splicing`). `RequiredTechValidator` never checks probe actions, so a typo’d `required_tech` on a probe action is the same silent-unavailable class of bug. Include this in Package 7 (id via `ProbeActionIdToString`).

**Amendment B — do not unify with `LoadGameData`:** Sharing one generated registry list across `LoadGameData` + both validators is higher churn than the bug warrants (different source sets: effect sources ≠ requiredTech sources ≠ load order). Fail-loud null checks + adding the missing probe walks is enough; keep parallel walk lists with updated docs/checklists. Revisit a shared `ForEach*` helper only if a third walker appears.

### Hygiene (in scope)

- Drop trailing `_` on anonymous-namespace free helpers (`.cursor/rules/coding-guidelines.md`: trailing `_` is for **private methods**).
- Sync header / `docs/architecture/effects-system.md` coverage lists with the actual walks (including probe actions, factions, council, tile-yield).

### Interactions

- **Package 5 (parser):** adding a new `EffectVariant_t` alternative will require a new visitor arm here — same “must touch the validator” step already documented in `effects-system.md:561-566`. Exhaustive `std::visit` makes that a **compile break**, which is the goal. Prefer landing Package 7 before or with Package 5 Pass 2 if Pass 2 adds types.
- **Package 3/8:** no shared code; no blocker.
- **Architecture doc** understates current coverage; update as part of this package.

---

## Design decision

### Chosen approach

1. **Exhaustive `std::visit`** over `rEffect.effect` with one `operator()` / overload **per** `EffectVariant_t` alternative. Id-bearing arms: `GrantBuildingEffect_t`, `GrantTechEffect_t`, `StatModifierEffect_t` (selector improvement only). All other alternatives (including `GrantUnitEffect_t`) get explicit empty arms. **No** generic `[](const auto&){}` catch-all — that would reintroduce silent omission.

2. **Split null policy by overload:**
   - **List overload** (`ValidateEffectReferences(effects, sourceId, pBuildings, …)`): keep nullable registry pointers; null = skip checks that need that registry (unit-test partial contexts). Update the header to state this clearly.
   - **`GameDataContext` overloads** (both validators): treat missing registries that `LoadGameData` always installs as **unexpected null → throw** `std::runtime_error` naming the missing field. Do not early-return. When calling the list overload, pass non-null pointers obtained after the null checks (or dereference into locals).

3. **Add probe walks:**
   - EffectReference: for each action in `probeActionsConfig->actions`, validate `action.effects` with source id `probe_action:<id-string>`.
   - RequiredTech: if `requiredTech` non-empty, require it in `TechRegistry`; source name via `ProbeActionIdToString(action.id)`.

4. **Hygiene:** rename free helpers without `_`; fix doc comments + architecture coverage bullets.

### Rejected alternatives

| Alternative | Why rejected |
|-------------|--------------|
| `switch` on a parallel `EffectType` enum | No such enum today; would duplicate `EffectVariant_t`. |
| Generic visitor catch-all + comments | Loses the compile-time guard the package exists for. |
| Require non-null registries on the **list** overload | Breaks existing partial-validation unit tests without benefit; production path is the `GameDataContext` overload. |
| Codegen / shared registry table with `LoadGameData` | Different membership sets; high churn for little gain (Amendment B). |
| Fold RequiredTech into EffectReferenceValidator | Distinct concern (scalar field vs effect-list ids); keep separate entry points, same null policy. |

---

## Implementation plan

1. **`EffectReferenceValidator.cpp` — per-effect dispatch**
   - Replace `get_if` chain with `std::visit` + local overloaded visitor (project has no shared `overloaded` helper; a local struct or C++17 overload set in the anonymous namespace is fine).
   - Keep `removedByTech`, condition, and `unitFilter` checks outside the visit (they are on `EffectConfig_t`).
   - Rename `ThrowBadReference_` → `ThrowBadReference`.

2. **`ValidateEffectReferences(const GameDataContext&)`**
   - At entry: if any of `buildingRegistry`, `improvementRegistry`, `techRegistry`, `unitComponentRegistry` is null → throw.
   - For each walked effect source that `LoadGameData` always populates (`popTypeRegistry`, `unitComponentRegistry`, `socialPolicyRegistry`, `socialRatingRegistry`, `factionRegistry`, `councilProposalRegistry`, `councilRules`, `probeActionsConfig`, plus buildings/improvements already required): null → throw (do not skip).
   - `tileYieldRules` remains a plain `vector` (always present; may be empty).
   - Add probe-actions loop after council rules (before or after `tileYieldRules`).

3. **`RequiredTechValidator.cpp`**
   - Remove early return; throw if `techRegistry` is null.
   - For each walked source registry that LoadGameData always populates: null → throw.
   - Add probe-actions `requiredTech` check (special-cased; id is `ProbeActionId_t`, not `std::string`).
   - Rename `ValidateRequiredTech_` → `ValidateRequiredTech` (template free function).

4. **Headers + architecture**
   - Sync walk lists in both validator headers.
   - Update `docs/architecture/effects-system.md` EffectReferenceValidator / RequiredTechValidator coverage sections (probe actions; drop “null TechRegistry skips” as the production contract; note list-overload null skip remains for tests).

5. **Tests (requirement-based — see below)**
   - Change GameDataContext null-tech / empty-context cases from “no-op OK” to “throws”.
   - Keep list-overload null-skip cases.
   - Add probe effect-id and probe required_tech cases.

---

## Test plan

Requirements, not “whatever the code does today.”

| Requirement | Fixture / assertion |
|-------------|---------------------|
| Unknown `GrantBuilding` / selector improvement / condition feature / `HasComponent` still throw naming source + id | Existing `tests/effects/ValidationTests.cpp` cases stay green |
| List overload: null registry skips only that check | Keep `ValidationTests.cpp` null-registry `CHECK_NOTHROW` cases |
| `GameDataContext` EffectReference: null `techRegistry` (or other required target registry) **throws** | New or updated test; must not silently succeed |
| `GameDataContext` RequiredTech: null `techRegistry` **throws** | Replace `RequiredTechValidatorTests.cpp:85` no-op case with throw expectation (requirement change) |
| Null source registry on RequiredTech when tech is present but buildings omitted | Prefer: throw if the GameDataContext path requires that registry; if keeping “absent source = nothing to check” for *optional* partial contexts, document — but after LoadGameData all sources exist, so production path throws on null sources |
| Probe action effect with unknown grant/tech/selector/condition id throws and names the probe action | New test building a minimal `GameDataContext` with registries + `probeActionsConfig` containing a bad id |
| Probe action `required_tech` unknown → throws naming action + tech | New RequiredTech test; known tech → no throw |
| Adding a new `EffectVariant_t` alternative without a visitor arm fails to compile | Manual/CI via build; no runtime test — note in PR |

Run via `./bd test` (filter `[validation]` / effects validation suite as appropriate). Do not invoke raw ctest.

---

## AI implementation prompt

```text
You are implementing Package 7 of the Alpha Centauri effects-model remediation at
/home/martok/alpha-centauri.

## Goal

Make load-time effect / required-tech reference validation exhaustive and fail-loud:

1. `EffectVariant_t` id checks use exhaustive `std::visit` (compile break when a new
   alternative is added without a visitor arm).
2. `GameDataContext` validator overloads throw on unexpected null registries; never
   silently no-op the whole check.
3. Walk `probeActionsConfig` effect lists in EffectReferenceValidator.
4. Walk probe-action `requiredTech` in RequiredTechValidator.
5. Fix doc-comment drift and trailing `_` on anonymous-namespace free functions.

Read first: `docs/effects-fix-prompts/07-effect-reference-validation.md` (this package’s
analysis). Follow `.cursor/rules/coding-guidelines.md`. Build/test only via `./bd`.

## Files to change

- `include/game/EffectReferenceValidator.h`
- `src/game/EffectReferenceValidator.cpp`
- `include/game/RequiredTechValidator.h`
- `src/game/RequiredTechValidator.cpp`
- `tests/effects/ValidationTests.cpp`
- `tests/game/RequiredTechValidatorTests.cpp`
- `docs/architecture/effects-system.md` (coverage / null-policy bullets only)

Do not change parsers, `LoadGameData` load order, or effect runtime code unless a compile
error forces a tiny include fix.

## Constraints

- SOLID / references over pointers where practical; throw on unexpected null (production
  `GameDataContext` path).
- No legacy shims; no “null means skip” on the GameDataContext overloads.
- List overload `ValidateEffectReferences(effects, sourceId, pBuildings, pImprovements,
  pTechs, pUnitComponents)` MAY keep nullable pointers for partial unit-test contexts;
  document that in the header.
- `GrantUnitEffect_t` remains intentionally unvalidated (no unit-design registry) — give it
  an explicit empty visitor arm, not a generic catch-all.
- Visitor must list every alternative in `EffectVariant_t`
  (`include/game/effects/BonusEffect.h`). No `[](const auto&){}` fallback.
- Trailing `_` is only for private methods — rename `ThrowBadReference_` /
  `ValidateRequiredTech_` accordingly.
- Probe effect source id format: `probe_action:<ProbeActionIdToString(id)>` (or equivalent
  clear stable string). RequiredTech messages should name the probe action similarly.
- Prefer not to invent a shared codegen table with `LoadGameData`; parallel walk lists +
  fail-loud nulls are enough.
- Keep RequiredTechValidator as its own component (do not merge into EffectReferenceValidator).

## Implementation steps

1. Refactor list-overload effect-payload checks to `std::visit` with exhaustive arms.
2. Harden `ValidateEffectReferences(const GameDataContext&)`: throw if required target
   registries or walked effect-source unique_ptrs are null; add probeActionsConfig walk.
3. Harden `ValidateRequiredTechReferences`: throw if techRegistry null; throw if walked
   source registries null; add probe requiredTech validation.
4. Update headers + architecture doc coverage lists.
5. Update tests per acceptance criteria (requirement change for GameDataContext null cases).

## Acceptance criteria

- [ ] New id-bearing `EffectVariant_t` alternative without a visitor arm fails to compile
      under the project’s normal `./bd build`.
- [ ] `ValidateRequiredTechReferences` / `ValidateEffectReferences(GameDataContext)` throw
      when `techRegistry` (and other required registries) are null — tests updated; the old
      “empty context is a no-op” case is gone.
- [ ] List-overload null-registry skip behavior retained and still tested.
- [ ] Probe action effects are id-checked; bad id throws naming the probe action.
- [ ] Probe action `required_tech` is checked; stock `gene_splicing` path remains valid;
      unknown tech throws.
- [ ] Header comments and `docs/architecture/effects-system.md` match the walks.
- [ ] `./bd test` passes for validation / effects tests you touch; report any failures.

## What not to do

- Do not unify validator walks with `LoadGameData` via codegen.
- Do not make the list-overload require non-null registries.
- Do not start validating `GrantUnit` against a non-existent registry.
- Do not change parser strictness (Package 5) or pool/runtime behavior (Packages 1–4, 6).
- Do not commit unless asked.
```
