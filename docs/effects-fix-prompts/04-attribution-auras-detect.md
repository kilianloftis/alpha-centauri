# Package 4 — Attribution, auras, Detect

**Date:** 2026-08-04  
**Source:** [`docs/effects-fix-packages.md`](../effects-fix-packages.md) Package 4; findings in [`docs/effects-model-review.md`](../effects-model-review.md)  
**Verdict:** Confirm the review fix direction, with two amendments (Conceal must gate on subject faction; Detect without `ownerFaction` must fail closed).

---

## Verified diagnosis

### 1. Unit-projected auras carry no faction attribution — **confirmed**

`AppendOwnedImprovementEffects_` stamps `ownerFaction` only for `ownedByTerritory` improvements (`TileEffectsContext.cpp:117-125`). Unit auras are collected separately:

```73:98:src/game/effects/TileEffectsContext.cpp
// ... Unit auras are not territory-owned.
void AppendUnitAuraEffects_(...)
{
    // ...
    for (ActiveEffect_t& rActive : pUnit->GetDesign().CollectEffects())
    {
        if (TileEffectReaches(*rActive.config, distance))
        {
            rOut.push_back(std::move(rActive));  // ownerFaction left unset
        }
    }
}
```

`CollectAreaEffects` (`:265-271`) concatenates own-tile + neighbor improvements + these unit auras. Comment at `:76` records the gap; it does not justify universal application.

Shipped latency: `Carrier_Deck` (`config/unit_components/abilities.json:93-109`) is the only production `ThisTile` unit effect today, and consumers reach it via `TileProvidesFlag` (`ActiveEffect.cpp:675-700`), which does its **own** per-unit faction check (`:686`) rather than `ownerFaction`. So current gameplay is accidentally correct; the first unit-component `Detect` / defense aura / `Conceal` aura will not be.

### 2. `Detect` with no `ownerFaction` reveals to every faction — **confirmed**

```22:25:src/game/faction/UnitVisibility.cpp
bool AppliesForFaction_(const ActiveEffect_t& rEffect, FactionId_t forFaction)
{
    return !rEffect.ownerFaction.has_value() || *rEffect.ownerFaction == forFaction;
}
```

`HasDetectionCovering_` (`:64-76`) uses that gate. Unset → every observer. Sensor works today only because `owned_by_territory` stamps the territory owner (`TileEffectsContext.cpp:124`); unowned Sensor stores `k_NoFactionOwner` (has_value true, matches nobody) — pinned by `UnitVisibilityTests.cpp:82-99`.

Residual hole beyond unit auras: fixture `conditional_cloak_detector` (`tests/fixtures/improvements.json:233-244`) declares `Detect` **without** `owned_by_territory`, so its Detect is currently universal. Existing tests only assert for the placer faction (`UnitVisibilityTests.cpp:236-256`).

### 3. `IsUnitVisibleTo` re-collects area effects per channel — **confirmed**

- `CollectConcealmentChannels_` calls `CollectAreaEffects` once (`UnitVisibility.cpp:45`).
- `HasDetectionCovering_` calls `CollectAreaEffects` again for **each** channel (`:64`), invoked from the channel loop in `IsUnitVisibleTo` (`:104-110`).

`CollectAreaEffects` allocates a fresh vector and walks the Chebyshev neighbourhood (improvements + units) every time. Callers include per-frame UI (`WorldView`, `UnitMarkerRenderer`) and O(tiles × units) order scanning.

### 4. Duplicated `AppliesForFaction_` — **confirmed**

Byte-identical helpers at `TileEffectsContext.cpp:167-171` (used by `ResolveTileDefenseMultiplier` `:386`) and `UnitVisibility.cpp:22-25`. Hygiene note in the review (`ActiveEffect.h:40` gate belongs next to the field) is accurate; the field comment (`ActiveEffect.h:37-40`) currently mentions only territory-owned improvements.

### 5. Amendment: Conceal path never uses the faction gate

`CollectConcealmentChannels_` (`UnitVisibility.cpp:45-56`) inserts every tile-area `Conceal` channel with **no** `AppliesForFaction_` check. Stamping `ownerFaction` on unit auras alone does **not** fix “`ThisTile` Conceal hides enemy units for free” — that requires filtering tile-area Conceal by the **subject** faction. Terrain Conceal (Fungus: unset `ownerFaction`) must remain universal.

---

## Design decision

### Chosen

1. **Stamp unit auras** in `AppendUnitAuraEffects_`: after a reachable effect is selected, set `ownerFaction = pUnit->GetFaction().GetFactionId()` before push. Attribution is the projecting unit’s faction, not territory.
2. **Export one public helper** next to `ActiveEffect_t::ownerFaction` in `ActiveEffect.h`, e.g. `AppliesForFaction(const ActiveEffect_t&, FactionId_t)` — same semantics as today for the general gate: unset ⇒ applies to all (terrain defense / Fungus Conceal). Delete both file-local copies.
3. **Fail-closed Detect**: in `HasDetectionCovering_`, require `rEffect.ownerFaction.has_value() && AppliesForFaction(rEffect, observerId)` (or equivalent). A `Detect` with no stamped owner never pierces. Update `conditional_cloak_detector` to `owned_by_territory: true` (and ensure tests place a base / claim territory as Sensor tests do). Document on `ownerFaction` / `UnitVisibility.h` that Detect sources must be attributed at collection.
4. **Gate tile-area Conceal** in `CollectConcealmentChannels_` with `AppliesForFaction(rEffect, subjectFactionId)` so only unset (terrain) or subject-owned auras conceal the subject. Live unit `ThisUnit` Conceal via `CollectLiveUnitEffects` is unchanged (already subject-scoped).
5. **Collect once** in `IsUnitVisibleTo`: one `CollectAreaEffects` for the subject tile; pass `const std::vector<ActiveEffect_t>&` into concealment and detection helpers.

### Rejected

| Alternative | Why not |
|-------------|---------|
| Parser requires `owner` on Detect | Owner is runtime (unit / territory); JSON cannot name it. |
| Change global `AppliesForFaction` to “unset ⇒ none” | Breaks intentional universal tile effects (Rocky defense, Fungus Conceal). |
| Only stamp units; leave Detect unset=all | Leaves non-`owned_by_territory` Detect improvements (and future mod mistakes) revealing to everyone. |
| Merge the two Chebyshev passes inside `CollectAreaEffects` | Package 6 hygiene; do not expand scope here. |
| Faction-gate yield/moisture unit auras | Out of scope; defense already gates via `AppliesForFaction`. Note only. |

### Package interactions

- **Independent of packages 1–3** for merge purposes; touches `ActiveEffect.h` surface (new free function) — coordinate if package 3 is rewriting that header heavily.
- **Package 6** may later unify Chebyshev traversal / yield API; keep `CollectAreaEffects` signature stable (`const Tile&` → vector).
- Update `docs/architecture/effects-system.md` unit-aura bullet and the stale `TileProvidesFlag` comment (`ActiveEffect.h:353-356`) that claims unit auras are “deliberately not territory-owned” — rephrase to: flags use an on-tile unit faction check; projected auras stamp `ownerFaction` from the unit.

---

## Implementation plan

1. Add `inline bool AppliesForFaction(const ActiveEffect_t& rEffect, FactionId_t forFaction)` beside `ownerFaction` in `ActiveEffect.h`; expand the field comment (territory-owned improvements **and** unit-projected `ThisTile` auras; Detect requires a stamped owner).
2. `AppendUnitAuraEffects_`: stamp `ownerFaction` from `pUnit->GetFaction().GetFactionId()`; drop/rewrite the “not territory-owned” comment to state faction attribution.
3. `TileEffectsContext.cpp`: remove local `AppliesForFaction_`; call the shared helper from `ResolveTileDefenseMultiplier`.
4. `UnitVisibility.cpp`:
   - Remove local `AppliesForFaction_`.
   - Refactor `CollectConcealmentChannels_` / `HasDetectionCovering_` to take the pre-collected area-effects span (plus subject/observer as needed).
   - Conceal (area): `AppliesForFaction(effect, subjectId)`.
   - Detect: require `ownerFaction.has_value()` and `AppliesForFaction(effect, observerId)`.
   - `IsUnitVisibleTo`: collect once, pass down.
5. Fixture: `owned_by_territory: true` on `conditional_cloak_detector`; adjust that test’s territory setup if needed (mirror Sensor cases).
6. Docs: `UnitVisibility.h` brief; `effects-system.md` unit-aura / attribution note; fix `TileProvidesFlag` comment.
7. Tests (below). Build/test only via `./bd`.

---

## Test plan

Requirement-based (encode intended rules, not today’s accidental universal auras).

| Requirement | Suggested case |
|-------------|----------------|
| Unit `ThisTile` Detect pierces Conceal **only for the projecting unit’s faction** | Fixture component with `Detect`/`ThisTile`/radius; owner’s observer sees cloaked enemy in radius; third faction with vision on the same tile does not. |
| Unit `ThisTile` Conceal conceals **only friendly** subjects in radius | Aura unit of A; enemy of B on adjacent tile does **not** gain the channel (visible to a scout of A who lacks Detect). |
| Terrain Conceal (Fungus) still hides every faction’s units | Existing fungus/Sensor cases stay green. |
| Sensor Detect still territory-gated | Existing `Sensor Detect requires territory ownership` / ownership-change cases. |
| Detect with no `ownerFaction` never covers | Non-territory Detect improvement without stamp (or explicit unset in a controlled fixture) does not pierce; after marking detector `owned_by_territory`, only the territory owner pierces. |
| Conditional Detect still condition-gated | Existing river condition case, with territory ownership. |
| Own units / contact reveal unchanged | Existing cloak / reveal cases. |
| Defense aura from unit benefits only owner faction | Optional but valuable: unit projects `Defense` `AddPercent` `ThisTile`; owner resolves higher multiplier than a foreign faction on the same tile (`ResolveTileDefenseMultiplier`). |
| No behaviour change for `TileProvidesFlag` / Carrier_Deck | Existing transport/refuel tests if any; otherwise smoke that enemy Carrier_Deck does not refuel you (already unit-faction-checked). |

Do **not** weaken existing Sensor/fungus assertions to green the suite.

---

## AI implementation prompt

```text
You are implementing Package 4 of the effects-model remediation for the Alpha Centauri C++ rebuild at /home/martok/alpha-centauri.

## Goals

1. Every unit-projected tile aura carries `ActiveEffect_t::ownerFaction` = the projecting unit’s faction id.
2. One shared `AppliesForFaction` next to `ownerFaction` (delete duplicates in TileEffectsContext and UnitVisibility).
3. `Detect` without a stamped `ownerFaction` never reveals concealed units (fail closed).
4. Tile-area `Conceal` only applies to subjects for whom `AppliesForFaction` is true (terrain Conceal stays universal via unset owner).
5. `IsUnitVisibleTo` calls `CollectAreaEffects` once per check and reuses the vector for concealment + detection.

Read for context (do not re-litigate): `docs/effects-fix-prompts/04-attribution-auras-detect.md`, Package 4 in `docs/effects-fix-packages.md`, and the cited findings in `docs/effects-model-review.md`.

## Constraints

- Follow `.cursor/rules/coding-guidelines.md` (references over pointers, throw over silent wrong defaults, no back-compat shims).
- Build and test only via `./bd` (never raw cmake/make/ctest).
- Do not hardcode game balance numbers in C++.
- Keep `FactionEffects_t` / `BaseEffects_t` lane typing untouched.
- Out of scope: merging Chebyshev passes inside `CollectAreaEffects` (package 6); yield/moisture faction filtering; parser strictness; pool rebuild; ActiveEffect constructor / Instantaneous Infiltration (package 3) except adding the shared `AppliesForFaction` helper and comment updates on `ownerFaction`.

## Files to change (expected)

- `include/game/effects/ActiveEffect.h` — `AppliesForFaction`; update `ownerFaction` and `TileProvidesFlag` comments.
- `src/game/effects/TileEffectsContext.cpp` — stamp unit auras; use shared helper; fix comments.
- `src/game/faction/UnitVisibility.cpp` / `.h` — single collect; Conceal/Detect gating; docs on Detect attribution.
- `tests/fixtures/improvements.json` — `owned_by_territory: true` on `conditional_cloak_detector`.
- `tests/game/UnitVisibilityTests.cpp` — new unit-aura Detect/Conceal attribution cases; keep Sensor/fungus requirements.
- Optionally a small defense-aura attribution test if cheap via existing fixtures.
- `docs/architecture/effects-system.md` — unit-aura attribution note (brief).

## Acceptance criteria

- Unit `ThisTile` Detect helps only the projecting faction’s observers.
- Unit `ThisTile` Conceal does not hide enemy units.
- Fungus Conceal and Sensor territory Detect behaviour unchanged in spirit (existing tests green, updated only if fixture territory requirements change for conditional detector).
- Detect with unset `ownerFaction` does not pierce.
- `IsUnitVisibleTo` performs one `CollectAreaEffects` per call (no per-channel rescan).
- `./bd test` with visibility/effects filters you touch passes; report any failures and whether they are requirement changes vs bugs.

## What not to do

- Do not change global “unset ownerFaction ⇒ applies to all” for non-Detect consumers (defense from Rocky, etc.).
- Do not require JSON `owner` fields for Detect.
- Do not refactor yield preview / ResolveYieldFromEffects_ copies (package 6).
- Do not commit unless asked.
```
