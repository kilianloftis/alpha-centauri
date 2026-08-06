# Package 6 — Tile yield resolution API

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 6; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** **Confirm** the review fix (preview = as-if-worked with selectors; cut hot-path copies). **Amend** scope slightly: delete `ResolvePreviewTileYield` rather than keep a third overload; add a total-only resolve helper for the tile hot path only; include free Chebyshev/max-radius consistency; defer WorkerAssignment sort and `(2r+1)²` caching.

---

## Verified diagnosis

### Preview vs worked disagree on selector modifiers — **confirmed**

| Claim | Evidence |
|-------|----------|
| Worked path appends matching selectors | `ResolveTileYield(tile, isBaseTile, rBaseEffects)` calls `AppendMatchingTileModifiers_` then resolve — `src/game/effects/TileEffectsContext.cpp:279-284` |
| Preview path skips selectors | `ResolvePreviewTileYield` forwards only `CollectAreaEffects` + caps — `:287-290` |
| UI uses preview for unworked tiles | `BaseWorkableAreaDisplay::RenderTile_` — worked → `GetWorkedTileYield`, else `GetPreviewTileYield` — `src/ui/base/BaseWorkableAreaDisplay.cpp:87-89` |
| BaseManager wires preview to the skip path | `GetPreviewTileYield` → `ResolvePreviewTileYield` — `src/game/faction/base/BaseManager.cpp:373-375`; worked → `WorkerAssignmentManager::GetWorkedTileYield` → full `ResolveTileYield(..., rBaseEffects)` — `:368-370`, `WorkerAssignmentManager.cpp:187-188` |
| Tests pin the discrepancy | After `gene_splicing`, worked effective nutrients `4`, preview `3` with comment `// Wet+Farm, no booster` — `tests/game/TileResourceRestrictionTests.cpp:106-107`. Pre-tech, preview `potential` is `3` vs worked `4` — `:100-101` |
| Fixture is a real Farm selector | `farm_booster` in `tests/fixtures/buildings.json:201-216` — `+1` nutrients, `HasImprovement`/`Farm` |
| Architecture documents the wrong split | `docs/architecture/effects-system.md:461-462` — preview described as “without building selectors” |

Selector matching itself does **not** check worker assignment (`AppendMatchingTileModifiers_` / `TileMatchesSelector_` at `TileEffectsContext.cpp:173-204`). Only the public overload choice omits the pass. A building that boosts Farms therefore shows the pre-boost number on every unworked Farm the player uses for placement — the review’s player-visible bug.

**Nuance (not a bug for this package):** `GetWorkedTileYield` also applies the assigned pop’s `ApplyTileMultipliers` (`WorkerAssignmentManager.cpp:187-192`). Preview has no pop. Unifying selectors makes tile-level yield match; pop-scaled worked display can still differ. That is correct — preview means “as if this tile were worked,” not “as if this specific pop worked it.”

### `ResolveYieldFromEffects_` copies three times; discarded breakdowns — **confirmed**

| Step | Evidence |
|------|----------|
| Copy 1 — always deep-copy effects | `std::vector<ActiveEffect_t> filtered = effects` — `TileEffectsContext.cpp:306` — even when `suppressYieldSources` is empty (normal case `:328-335`) |
| Copies 2–3 — lane split | `beforeRestriction` / `afterRestriction` push every element again — `:337-353` |
| Six resolve calls build sorted breakdowns | `ResolveResource_` ×3 ×2 lanes — `:357-366` → `ResolveStatModifiers` — `:293-299`. That template always materializes + sorts `contributions` — `include/game/effects/ActiveEffect.h:186-225`. Totals only need `ApplyModifierStack` (`ActiveEffect.cpp:218-232`), which is order-independent (add / % sum / geometric product). |

This partially undoes the 2026-07-09 lazy-filter work (`docs/code-review-findings.md` ~44–65).

### Related items (verified; scoped below)

| Item | Evidence | Package 6? |
|------|----------|------------|
| Two Chebyshev walks per `CollectAreaEffects` | Neighbors `:128-147`, units `:77-97`; both called from `:265-271` | Yes — merge if free |
| `m_maxRadius` rule inconsistency | Improvements: max over **all** effect radii (`:231-234` via `MaxEffectReach_`); units: only `ThisTile` (`:239-244`) | Yes — align to ThisTile-only for both |
| Doc Manhattan vs code Chebyshev | `effects-system.md:137-138` vs `ForEachTileInChebyshevRadius` at `:80` | Yes — doc fix |
| WorkerAssignment sort re-resolves yields | Scorer inside `std::sort` comparator — `WorkerAssignmentManager.cpp:324-330`; default scorer calls `ResolveTileYield` — `:26-31` | **Defer** (optional follow-up) |
| `(2r+1)²` area scans | Known gap; review defers | **Defer** |
| Global `ResolveStatModifiers` always sorts | `ActiveEffect.h:212-216`; package 1 may touch resolve family | Total-only helper for **tile hot path**; do not rewrite all call sites here |

### Package interactions

- **Package 1** (single pool / % seed / rounding): preview/worked both use `BuildBaseEffects_()` today. Pool completeness is package 1; this package only makes preview and worked use the **same** resolve path for whatever pool they receive. Prefer landing after or with package 1 if both touch `BaseManager` yield getters, but no hard blocker.
- **Package 4** (aura attribution): shares `CollectAreaEffects` / Chebyshev helpers — coordinate if both edit neighbour walks; attribution (`ownerFaction` on units) is out of scope here.
- **Package 5** (parser): tile-yield rule validation out of scope.

---

## Design decision

### Chosen

1. **Preview = as-if-worked for tile-level yield.** Same selector pass, same cap / `apply_after_restriction` assembly as `ResolveTileYield(tile, bIsBaseTile, rBaseEffects)`.
2. **Delete `ResolvePreviewTileYield`.** `BaseManager::GetPreviewTileYield` calls `ResolveTileYield(rTile, /*bIsBaseTile*/ false, BuildBaseEffects_())`. No third public overload; no nullable `pCapEffects`.
3. **Keep the no-base-effects overload** `ResolveTileYield(const Tile&)` for intrinsic+area only (`effective == potential`) — used by auto-assign scorer (`WorkerAssignmentManager.cpp:30`) and many tests.
4. **Private resolve API:** replace `ResolveYieldFromEffects_(..., const BaseEffects_t* pCapEffects)` with two overloads (or a required `const BaseEffects_t&` plus a no-cap path that does not take caps) — references only, no optional raw pointer.
5. **Hot-path copies:** build suppress set first; if empty, partition from the original `effects` without an unconditional clone; if non-empty, one filtered vector then partition (prefer pointer/index partitions or a single pass into two lanes over three full `ActiveEffect_t` copies).
6. **Total-only resolve for `ResolveResource_`:** add a small helper (e.g. `ResolveStatModifiersTotal` next to `ResolveStatModifiers` in `ActiveEffect.h`) that accumulates `{amount, op}` and returns `ApplyModifierStack` **without** sorting or storing `StatBreakdown_t::Contribution`. Tile yield uses it; callers that need display breakdowns keep `ResolveStatModifiers`.
7. **Include free consistency fixes:** one Chebyshev traversal for improvement+unit auras in `CollectAreaEffects`; size `m_maxRadius` with the same ThisTile-radius rule for improvements and unit components; rename `isBaseTile` → `bIsBaseTile` on the public API; fix architecture doc (Chebyshev + preview semantics).

### Rejected / deferred

| Alternative | Why not |
|-------------|---------|
| Keep `ResolvePreviewTileYield` as thin alias forever | Invites the same drift; guidelines: no back-compat shims — update call sites |
| Preview without caps but with selectors | Caps are part of “what you get if you work this tile”; keep full assembly |
| Make preview apply pop multipliers | No pop on unworked tiles; wrong API |
| Cache `(2r+1)²` neighbourhood | Known-gap; expensive design; out of package |
| Fix WorkerAssignment sort comparator in this PR | Optional; separate performance follow-up |
| Change all `ResolveStatModifiers` call sites to total-only | Scope creep into package 1 / unrelated hot paths |

### Requirement change (tests) — **explicit**

`tests/game/TileResourceRestrictionTests.cpp` currently **requires** preview to exclude Farm selectors (`:101`, `:107`). That was pinning a bug, not a product rule.

**New requirement:** for the same tile and same `BaseEffects_t`, preview tile-level yield equals worked tile-level yield from `ResolveTileYield` (before pop multipliers). Update those assertions; do not delete the case — retarget it to prove preview includes `farm_booster`.

---

## Implementation plan

1. **`ActiveEffect.h`:** add `ResolveStatModifiersTotal(Range&&, double baseValue, const EffectContext_t* = nullptr)` → `double` via `ApplyModifierStack`, no sort/breakdown.
2. **`TileEffectsContext`:**
   - `ResolveResource_` → total-only helper.
   - Refactor `ResolveYieldFromEffects_` (suppress + partition without triple copy; drop `pCapEffects` pointer).
   - Make worked overload the single base-effects entry; delete `ResolvePreviewTileYield` declaration/definition.
   - Rename `isBaseTile` → `bIsBaseTile` (header `:51`, cpp, call sites).
   - Optional-in-scope: merge Chebyshev walks; unify `m_maxRadius` ThisTile rule.
3. **`BaseManager::GetPreviewTileYield`:** call `ResolveTileYield(rTile, false, BuildBaseEffects_())`.
4. **Docs:** `effects-system.md` — radius is Chebyshev; preview documents as-if-worked / same as worked overload with `bIsBaseTile == false`; remove “without building selectors.”
5. **Tests:** update `TileResourceRestrictionTests` (requirement change); add/adjust a focused case that unworked preview includes selector boost; keep existing selector tests in `TileEffectsTests.cpp` as the contract for the worked overload.

---

## Test plan (requirement-based)

Use `./bd test` with relevant filters (e.g. `[resources][restrictions]`, `[effects][tile][yield]`).

| # | Requirement | Assertion sketch |
|---|-------------|------------------|
| 1 | Preview is as-if-worked for selectors | Farm + `farm_booster` + caps lifted: `GetPreviewTileYield(farm).effective.nutrients == GetWorkedTileYield(farm).effective.nutrients` when the working pop has identity tile multipliers (or compare both to `ResolveTileYield(..., false, effects)`). Replace `:107` expecting `3` with expecting `4` / equality. |
| 2 | Preview potential includes booster under cap | Pre-`gene_splicing`: preview `potential.nutrients == 4` (was `3` at `:101`); effective still capped (`2`). |
| 3 | Caps still apply on preview | Existing effective-capped checks (`:98-99`) remain. |
| 4 | Flat non-selector bonuses still not per-tile | Unchanged — `TileEffectsTests` “flat modifiers are NOT applied per tile”. |
| 5 | No-base-effects overload still uncapped | `ResolveTileYield(tile)` → `effective == potential`; scorer path unchanged. |
| 6 | Selector / BaseTile / aura / suppress fixtures | Existing `TileEffectsTests` yield cases stay green (behavior preserved except preview API deletion). |
| 7 | Call-site compile | No remaining references to `ResolvePreviewTileYield`. |

**Do not** weaken production / restriction assertions to get green — only change expectations that encoded the old “preview omits selectors” rule.

---

## AI implementation prompt

```text
Implement Package 6 — Tile yield resolution API — for the Alpha Centauri C++ rebuild
at /home/martok/alpha-centauri.

## Goal
1. Unworked-tile preview yield must match worked tile-level yield for selector-carrying
   StatModifiers and the same TileResourceCap / apply_after_restriction assembly
   (“preview” = as if the tile were worked).
2. Stop deep-copying ActiveEffect_t lists three times per tile and stop building sorted
   StatBreakdown_t contributions on the tile yield hot path when only .total is used.
3. Update architecture docs and tests. This is an explicit **requirement change** for
   tests that pinned the old preview≠worked discrepancy.

## Verified bug (do not re-litigate)
- Worked: TileEffectsContext::ResolveTileYield(tile, isBaseTile, baseEffects) appends
  AppendMatchingTileModifiers_ (TileEffectsContext.cpp:279-284).
- Preview: ResolvePreviewTileYield skips that pass (:287-290).
- UI: BaseWorkableAreaDisplay.cpp:87-89 uses GetPreviewTileYield for unworked tiles.
- BaseManager.cpp:373-375 wires preview to the skip path.
- tests/game/TileResourceRestrictionTests.cpp:101 and :106-107 pin preview without
  farm_booster; that is wrong under the new requirement.
- ResolveYieldFromEffects_ (:306, :337-353) copies effects; ResolveResource_ →
  ResolveStatModifiers (ActiveEffect.h:186-225) sorts unused breakdowns six times/tile.

## Design (follow exactly)
1. Delete ResolvePreviewTileYield (header + cpp). No compatibility shim.
2. BaseManager::GetPreviewTileYield(rTile) must call
   m_rTileEffects.ResolveTileYield(rTile, /*bIsBaseTile*/ false, BuildBaseEffects_()).
3. Keep ResolveTileYield(const Tile&) (no base effects, no caps) for the auto-assign
   scorer and intrinsic+area callers.
4. Replace ResolveYieldFromEffects_(..., const BaseEffects_t* pCapEffects) with a
   reference-based API (overloads: with caps vs without). No nullable pointer.
5. Suppress filter: do not unconditionally `filtered = effects`. If suppress set empty,
   partition from the original list; if non-empty, one filter pass then partition.
   Prefer not cloning ActiveEffect_t into three vectors.
6. Add ResolveStatModifiersTotal (or equivalent name) beside ResolveStatModifiers in
   ActiveEffect.h: same filter loop + ApplyModifierStack, no sort, no Contribution
   vector. Use it from ResolveResource_ only in this change. Leave other
   ResolveStatModifiers callers alone unless trivially adjacent.
7. Rename isBaseTile → bIsBaseTile on the public ResolveTileYield overload; update
   call sites (ResourceManager, WorkerAssignmentManager, tests).
8. In-scope consistency (same PR if small):
   - One Chebyshev neighbourhood walk for CollectAreaEffects (improvements + units).
   - m_maxRadius: only consider ThisTile-scoped radii for improvements (same as units).
9. Docs: docs/architecture/effects-system.md — (a) radius is Chebyshev not Manhattan
   (~:137-138); (b) document preview as as-if-worked / same path as worked overload with
   bIsBaseTile false; remove “without building selectors” (~:462).

## Out of scope
- Package 1 (world/council pool fold, base-level % seed, rounding).
- Package 4 (stamp ownerFaction on unit auras / Detect).
- WorkerAssignmentManager sort comparator re-resolving yields (optional later).
- Caching (2r+1)² area scans.
- Rewriting every ResolveStatModifiers call site in the codebase.
- Pop ApplyTileMultipliers on preview (preview stays tile-level only).

## Constraints
- Follow .cursor/rules/coding-guidelines.md (references over pointers, throw over silent
  defaults, no legacy shims, naming: bIs*, r*, p*).
- Build/test only via ./bd (never raw cmake/make/ctest).
- Do not hardcode game balance numbers.
- Keep FactionEffects_t / BaseEffects_t lane typing intact.

## Acceptance criteria
- GetPreviewTileYield and ResolveTileYield(..., false, sameEffects) agree on
  effective/potential for Farm + farm_booster (± caps), for unworked placement UX.
- TileResourceRestrictionTests updated for the new requirement (preview includes
  booster); case retained, expectations corrected — not deleted to silence failure.
- No ResolvePreviewTileYield symbol left.
- Tile yield hot path does not unconditionally triple-copy effects or sort breakdowns
  for totals.
- ./bd test passes for tile/yield/restriction-related suites; full ./bd test if practical.
- effects-system.md matches Chebyshev + preview semantics.

## Test plan notes
- REQUIREMENT CHANGE: TileResourceRestrictionTests.cpp:101 (preview potential 3→4 with
  booster under cap) and :107 (preview effective 3→4 after gene_splicing, or equality
  with worked tile-level yield). Comment “Wet+Farm, no booster” must go.
- Keep flat non-selector-not-per-tile and existing TileEffectsTests selector cases.
- Mention in the PR/commit summary that preview tests changed because the product
  requirement changed, not to chase greens after a regression.
```
