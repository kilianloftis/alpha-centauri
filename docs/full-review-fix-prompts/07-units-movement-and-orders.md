# Package 7 — Units: model, orders, movement, path performance

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 7
**Verified against:** working tree at commit `7547934` (after full-review Packages 1–6)

**Status: mostly complete.** Both `[H]` findings and four of five `[M]`s are fixed and tested.
One `[M]` (`NextStep` full Dijkstra) is deferred with rationale.

---

## Done

### [H] Stacking left the position index; the process-global is an incomplete prior fix — FIXED

`MovementRules.cpp` held `s_bSingleUnitPerTile` at file scope, consulted by `CanPlaceUnitOnTile`
and `StepEvaluator`. Two problems: any caller that moved without going through the step check
could overstack (`UnitPositionIndex::MoveUnit` did not check capacity), and the setting was
process-wide, so two worlds could not disagree and a test could leak it into the next case —
which is why `UnitPositionTests` needed an RAII guard.

The rule now lives on `UnitPositionIndex`, beside the occupancy it constrains, and is enforced
in `MoveUnit` itself — the same mutation boundary that updates occupancy. Embarked passengers
are exempt: they ride with their carrier and are not independent occupants. The test guard is
gone; a test sets the rule on its own fixture's index.

### [H] Stop scanning the whole map for hostiles on every step — FIXED

`CollectVisibleHostileIds_` walked every tile of the map and every unit on it, and `TryStep`
calls it twice per attempted step (before and after), so a multi-step move paid
O(steps × tiles × units) — dominated by empty fog on a real map.

It now sweeps `UnitPositionIndex`, which holds only *occupied* tiles, via a new `ForEachUnit`.
That is O(units) per sweep instead of O(tiles × units).

**Not fully closed:** it is still O(units) per step. Restricting the sweep to units whose
visibility could have changed (a dirty set invalidated by reveal/move) would make it
proportional to what actually moved, but needs a visibility-revision seam that does not exist
yet. A TODO records this at the call site.

### [M] Terraform completion ignores apply failure after energy was spent — FIXED (reported, not refunded)

`ApplyTerraformResult`'s `bool` was discarded and the order always completed, as did the two
early exits (improvement id missing from the registry, unit not on a valid tile). Energy was
debited in `TryStartTerraform` and the turns were already spent, so the player paid for a
project that mutated nothing, silently.

All three exits now report. **Deliberately not refunded**: whether a pre-empted former gets its
energy back is a SMAC rule this codebase does not have, so the failure is made visible rather
than guessed at (`.devin/rules/coding-guidelines.md`). A TODO records the open decision.

### [M] Sea-former domain rules hardcode improvement ids — FIXED

`DomainAllows_` special-cased `KelpFarm` / `MiningPlatform` / `TidalHarness` by id, while
`config/improvements.json` already tags all three `sea_terraform`. A modded sea improvement with
the tag but a new id was treated as land-only. Now driven by the tag.

### [M] `EmbarkInto` does not enforce carrier invariants — FIXED

`Unit::EmbarkInto` linked cargo with no same-tile, capacity, domain or faction check —
`TransportRules` documented those as caller duties, so one missed call site could overfill
`m_cargo` (making `FreeCargoSlots` negative) or link a passenger to a carrier on another tile,
which `MoveUnit` would still tow. It now enforces same-tile and `CanCarryPassenger` (which
covers capacity, domain, faction, and carrier-is-not-itself-cargo), plus a self-carry guard.

### [M] Conquest depends on a post-ctor nullable `GameDataContext` — FIXED

With `m_pWorld` bound but `m_pGameData` unset, `ApplyArrivalEffects_` and post-combat conquest
silently returned: no capture, no native raid, no diagnostic. No world at all remains a
legitimate mode (movement-only harnesses); a world *without* game data is a wiring error and now
throws saying so.

---

## Deferred

- **[M] `NextStep` always runs a full Dijkstra.** `Pathfinder::NextStep` calls `FindPath` and
  returns `tiles.front()`, so every move fragment pays a full O(tiles log tiles) search plus two
  `tileCount`-sized vectors. The fix (early-exit Dijkstra that stops once the first step off the
  origin is finalized, or a search returning only the successor) is a self-contained pathfinder
  change, but it touches the search core that movement, ZOC and the AI all depend on, and it
  wants its own before/after benchmark to show the semantics are unchanged. Sequencing it with
  the remaining map/pathfinding work (package 10) keeps that verification in one place.
