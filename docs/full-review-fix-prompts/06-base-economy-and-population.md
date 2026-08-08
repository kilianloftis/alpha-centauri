# Package 6 — Base economy: worker assignment, resources, production, population composition

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 6
**Verified against:** working tree at commit `6e25eb8` (after full-review Packages 1–5)

**Status: PARTIAL.** Both `[H]` findings plus three `[M]`s are fixed and tested. The remaining
findings are listed below with what each needs; they are not started.

---

## Done

### [H] `IsTileAssigned` answers "worked by anyone" to callers asking "worked by me" — FIXED

`WorkerAssignmentManager::IsTileAssigned` forwards to the world-scoped `WorkedTileIndex`, which
is right for `GetAvailableTiles_` and wrong for both UI callers. For a tile inside this base's
radius worked by a neighbouring base, another faction, one of this faction's supply crawlers, or
that is another base's centre tile, it answered "worked" — and `GetWorkedTileYield`, which only
resolves against *this base's* pops, then returned zeros. The player saw `0 0 0` in worked-tile
colour on a tile that yields. Overlapping radii are the normal case, so any two-base start hits
it. `BaseView::HandleTileClick_` had the mirror problem: it routed such clicks to
`UserUnassignTile`, which scans only this base's pops and silently did nothing.

Fixed by adding `IsTileWorkedByThisBase` (a pop scan over the same set `GetWorkedTileYield`
uses, so the two cannot disagree) and pointing both UI callers at it. `IsTileAssigned` keeps its
world-scoped meaning for the availability check it was written for; the header now states which
question each answers.

### [M] `UserAssignBestAvailableWorker` ignores every failure and can strand a worker — FIXED

The fallback branches convert a specialist back to a plain worker, or pull a worker off a tile
it was productively working, and then discarded `UserAssignWorker`'s `bool`. A doomed request
therefore destroyed a specialist's role or left a worker idle for nothing. Now the tile is
checked for workability and freedom *before* any pop is touched, the method returns `bool`, and
a failed follow-up assignment triggers auto-assignment rather than stranding the pop.

### [M] Energy allocated to psych is a silent sink — DOCUMENTED, NOT FIXED

Confirmed: `ConsumePsych` has no caller anywhere, while `Faction` drains `ConsumeEcon` and
`ConsumeLabs` every turn. The player pays the psych share of the energy split and gets nothing.

Deliberately left as a TODO on the method rather than fixed. What psych *buys* (drone
suppression / talent creation) and how it should feed `PopCompositionCalculator` — whose
`psych_output` input is currently specialist psych only, not base psych production — is a SMAC
rule this codebase does not have. `.devin/rules/coding-guidelines.md` forbids inventing it.

---

### [H] `PopContainer` owns composition policy and rules services, not storage — FIXED

The container held the availability calculator and applied it in `ConvertToFallback` but not in
`ConvertTo`, so `ConvertTo(rPop, "Thinker")` installed a type the fallback path would refuse —
the tech gate existed on one conversion path only.

`PopContainer` is now storage (vector, counts, revision; registry-only constructor).
`PopContainer::ConvertTo` takes an already-resolved `PopTypeConfig_t`. `PopulationManager` owns
the policy: `ResolveType_` walks the obsolescence chain and is the single place a requested type
becomes an actual one, and `ApplyCompositionTargets` (the reconciliation loops) moved up with
it. The loops use `ConvertResolved_` so they cannot re-enter recalculation through `ConvertTo`'s
specialist hook.

### [M] `~BatchCompositionUpdate` runs work that can throw — FIXED

Deferred `RecalculateComposition` reaches the registry and the obsolescence chain, both of which
throw on an unsatisfiable config; destructors are implicitly `noexcept`, so that called
`std::terminate` — from a guard that sits on the hot worker-assignment paths. Now caught and
reported: stale composition is recoverable, termination is not.

### [M] Golden-age inputs use `GetWorkerCount()` — FIXED

That count is every tile-capable pop, so drones and talents landed on both sides of the
documented `talents >= workers + specialists` rule and the effective condition became "every pop
must be a talent". Added `GetPlainWorkerCount` for the disjoint bucket; both counts now document
what they include.

---

## Second pass (2026-08-08)

Four of the seven remaining `[M]`s are now addressed; the reasoning for the other three is below.

### [M] `RemovePop` is silent, arbitrary, and unobservable — FIXED (the first two)

`PopulationManager::OnPopRemoved` now carries the `Pop&` and fires **before** the erase, so an
observer can drop its reference while it is still valid — the same guarantee
`UnitManager::OnUnitDestroyed` has provided for units since the lifetime work. `OnPopLost(int)`
still follows with the new size. Removing from an empty base now throws instead of silently
doing nothing.

That throw needed its landing traced, and it had one: `BaseManager`'s starvation handler called
`RemovePop()` unguarded, and starvation fires whenever the stockpile goes negative *regardless of
size* — so a size-1 base starving twice would have crashed. Nothing removes a base at size zero,
so this is reachable in a normal game. The handler now returns early at size zero, with a TODO
that SMAC destroys such a base and that implementing it needs the rule for its buildings,
garrison and worked tiles.

**Still open:** *which* pop is removed. `pop_back` takes the most recently added, which after
reconciliation can be a talent while drones remain. That is a game rule this project has not
written down, so it is a TODO at the call site rather than a guess.

### [M] Composition goes stale on every size change except growth — FIXED

`RemovePop` now reconciles. Conquest, probe pop-kills and starvation all shrink a base mid-turn,
and the drone/talent split used to keep describing the old size until the next Population stage.

**Deliberately not applied to `AddPop`:** the first attempt did, and it broke `AddPop("Talent")` —
the caller names a type, and reconciling inside the add silently overwrites it with whatever the
formula says. Reconciliation belongs where the size moves with no caller intent about types.

### [M] The production queue has no contract — PARTLY FIXED

The contract is now stated on `SetProduction`: the mineral stockpile is never touched, so
switching carries the full stockpile and only completion spends it. Setting the item already
queued no longer re-announces a change.

**Still open, and it is a rule:** SMAC penalises switching between production categories. Carrying
the full stockpile is the placeholder, not a decision, and it is marked as such. Validating the
item against what the base may build cannot live in `ProductionManager` either — `BaseManager`
owns that question and its availability calculator is optional.

### [M] The `precedence` config key is parsed and ignored — ALREADY CLOSED

Re-checked: `PopCompositionConfigParser` *throws* on a non-empty `precedence`
("not implemented; remove it"). The key is rejected loudly, not silently ignored, so the finding
as written no longer describes the code. The hardcoded drone-then-talent order remains, with its
TODO.

## Not started

Each of these is confirmed present; none is addressed.
- **[M] Composition's `psych_output` is specialist psych only** (see the psych TODO above —
  same rule gap).
- **[M] Production's minerals-per-row is the one game number still in code**
  (`ProductionCostCalculator::k_MineralsPerRow`). Moving it needs a production config file and
  parser; there is no existing config to hang it on.
- **[M] `BaseSnapshot_t` carries an untyped production id** that only round-trips for buildings.
