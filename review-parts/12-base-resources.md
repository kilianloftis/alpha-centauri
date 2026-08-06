## Base management — resources and worker assignment

**Files:** `src/game/faction/base/resources/ResourceManager.cpp`, `include/game/faction/base/resources/ResourceManager.h`, `src/game/faction/base/resources/WorkerAssignmentManager.cpp`, `include/game/faction/base/resources/WorkerAssignmentManager.h`

**Assessment:** Prior finding 2.1 ("worker state in three classes") is genuinely fixed, and the
replacement is good: the authoritative record of "which tiles are worked" is `WorkedTileIndex`
(`include/game/map/WorkedTileIndex.h:65`, owned by `WorldMap`), an assignment *is* the RAII
`WorkedTileClaim` held by the `Pop` (`include/game/population/pop-types/Pop.h:81`), and "which
tiles *this base* works" is derived by iterating `m_rPops.Pops()` — there is no cached per-base
list to desync, and every mutation path (`Pop::Convert`, pop death, base founding, unit crawl
claims) releases through the same claim destructor, so the index cannot drift. What remains weak
is the *query* surface built on top of it: `IsTileAssigned` answers a world-wide question to
callers asking a per-base one, and `ResourceManager` is a thin but sloppy layer — six nullable raw
pointers, five near-identical stockpile accessors, and a base-level modifier pass that silently
drops every percentage effect.

### [H] Base-level percentage modifiers are silently discarded
`src/game/faction/base/resources/ResourceManager.cpp:124-126` resolves base-level stat modifiers
against a **zero** seed and then adds the result to the worked total:

```cpp
double base = static_cast<double>(GetResourceValue_(worked, stat));
base += ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, stat), SeedFor(stat)).total;
```

`ApplyModifierStack` computes `(seed + adds) * arithmeticFactor` (`src/game/effects/ActiveEffect.cpp:218-233`),
so with `SeedFor(Nutrients) == 0.0` an `AddPercent` contribution multiplies zero and vanishes.
This is not hypothetical: `config/social_rating_effects.json:19,23` gives the Economy rating levels
2/3 an `AllOwnerBases`, selector-free `minerals -10% / -20%` penalty, and
`TileEffectsContext::AppendMatchingTileModifiers_` deliberately forwards only *selector-carrying*
modifiers to the per-tile pass (`src/game/effects/TileEffectsContext.cpp:186-204`) — so that penalty
is applied nowhere in the game. `CalculateEcon_/Labs_/Psych_` (lines 145, 153, 161) have the same
shape, so any future "+50% labs at this base" facility will also be a no-op. Fix: seed the resolve
with the value being modified (`ResolveStatModifiers(filter, static_cast<double>(worked))`), exactly
as `Pop::ApplyTileMultipliers` already does (`src/game/population/pop-types/Pop.cpp:110-111`).

### [H] `IsTileAssigned` answers "worked by anyone" to callers asking "worked by me"
`src/game/faction/base/resources/WorkerAssignmentManager.cpp:142-148` forwards to the world-scoped
index, which is correct for `GetAvailableTiles_` but wrong for the two UI callers, and the class
offers no per-base alternative. `BaseWorkableAreaDisplay::RenderTile_` uses it to choose between
`GetWorkedTileYield` and `GetPreviewTileYield` (`src/ui/base/BaseWorkableAreaDisplay.cpp:71,87-89`);
for a tile inside this base's radius that is worked by a neighbouring base, by another faction, by
one of this faction's supply crawlers, or that is another base's centre tile, the branch says
"worked" and `GetWorkedTileYield` then finds no matching pop and returns zeros
(`WorkerAssignmentManager.cpp:194`) — the player sees `0 0 0` in worked-tile colour for a tile that
in fact yields. Overlapping radii are the normal case, so this is reachable in any two-base start.
`BaseView::HandleTileClick_` has the mirror problem (`src/ui/base/BaseView.cpp:189-196`): it routes
such a click to `UserUnassignTile`, which scans only this base's pops and silently does nothing.
Fix: add an explicit "worked by this base" predicate (a pop scan, or `GetWorkedTileYield` returning
`std::optional`) and keep `IsTileAssigned` for the availability check it was written for.

### [M] `ResourceManager` takes six nullable pointers and re-checks them at every use
`include/game/faction/base/resources/ResourceManager.h:23-29,53-58` — every collaborator is a raw
`const T*`, so the constructor cannot produce a valid object and the class defends itself four
separate times (`.cpp:95-98`, `141`, `151`, `159`) with the same throw, while `m_pBaseTile` and
`m_pHomeUnits` are instead treated as *optional* (`.cpp:100,109`): a base constructed without its
centre tile silently produces less food forever rather than failing. `m_pBuildings` is stored and
never read at all. All six are non-null in the only construction site
(`src/game/faction/base/BaseManager.cpp:83-85`). Fix: take references for what is required, drop
`pBuildings`, and delete the scattered checks — guidelines call for constructors that produce valid
objects and for throwing rather than silently degrading.

### [M] `UserAssignBestAvailableWorker` ignores every failure and can strand a worker
`src/game/faction/base/resources/WorkerAssignmentManager.cpp:224-252` — all three strategies discard
`UserAssignWorker`'s `bool`. The last one is destructive: it unassigns the lowest-yield worker and
then tries to claim `pTile`; if the claim fails (tile not in the workable set, or claimed between
the caller's check and here) the worker has lost its tile and gained nothing, and the caller is
told nothing. The specialist branch is similar — it converts a specialist to a worker and leaves it
idle if the assignment fails. `BaseManager::UserAssignBestAvailableWorker` is a `void` pass-through
(`src/game/faction/base/BaseManager.cpp:189-192`), so the UI cannot report the failure either. Fix:
check the result and restore the previous assignment (or return a bool up to the UI).

### [M] The displaced-worker handler outlives the object it captures
`Assign_` hands `TryClaim` a `[this]{ AutoAssignWorkers(); }` callback
(`WorkerAssignmentManager.cpp:93-94`) that is stored inside the claim, i.e. inside the `Pop`.
`WorkedTileIndex.h:13-16` states the handler "must remain valid for the claim's whole lifetime", but
`BaseManager` constructs `m_pPopulation` before `m_pWorkerAssignments`
(`src/game/faction/base/BaseManager.cpp:75-80`), so the manager is destroyed *first* and every
surviving pop then holds a claim whose handler points at freed memory. Nothing invokes it in that
window today (only `ClaimDisplacing` fires handlers, and pop destruction does not), so this is a
latent hazard rather than a live crash — but the ordering cannot be fixed by reordering members
(the manager needs the population to construct). Fix: give `WorkerAssignmentManager` a destructor
(currently `= default`, `.h:30`) that clears every pop's claim.

### [M] Energy allocated to psych is a silent sink
`ConsumePsych()` (`ResourceManager.cpp:208-213`) has no caller anywhere outside tests — `IncomeCollection`
and `ResearchAccumulation` consume econ and labs (`src/game/Faction.cpp:81-101`), nothing consumes
psych. Because econ is the *residual* bucket (`src/game/faction/EconomyManager.cpp:48-56`), raising
the psych percentage provably removes energy from the treasury each turn and produces nothing: the
riot/golden-age path reads only specialist psych via `PopContainer::ComputePsychOutput`, never this
stockpile. This is the "stub wired so it silently produces wrong values" case rather than a missing
feature — the cost is already charged. Related: the stockpiles are accumulate-until-consumed with
no per-turn guard, so a mod that drops a consuming stage from `config/turn_stages.json` grows them
without bound silently.

### [M] `int` truncation where the rest of the pipeline rounds
`ResourceManager.cpp:126` ends `CalculateResource_` with `static_cast<int>(base)`, and lines 145/153/161
truncate the modifier totals the same way. `Pop::ApplyTileMultipliers` uses `std::round`
(`src/game/population/pop-types/Pop.cpp:111`) for the equivalent step. Truncation toward zero loses a
whole resource point on ordinary binary-float products (e.g. a correctly applied `-30%` on 7 gives
4.899…→4, not 5) and rounds negative penalties *up*, i.e. in the player's favour. Once the percentage
bug above is fixed this becomes visible on every base. Fix: pick one rounding rule for resource
integers and use it in both places.

### [L] Convention and hygiene items
- `WorkerAssignmentManager.h:50-57` — `ReleaseUserAssignment` / `ReleaseAllUserAssignments` have no
  callers anywhere (the latter not even in tests) and `ReleaseUserAssignment`'s body is identical to
  `UnassignWorker` (`.cpp:56-59` vs `103-106`); two public names for one operation, with docs that
  imply a distinction the claim model cannot express (the user flag lives on the claim, so it cannot
  be released without freeing the tile).
- `WorkerAssignmentManager.h:90-92` — `SetTileScorer` has no callers; it is the only reason `m_scorer`
  is a `std::function` (`.h:111`) rather than a private helper. Speculative generality.
- `WorkerAssignmentManager.cpp:319-333` — `PrioritizeAvailableTiles_` copies its argument and calls
  `m_scorer` (a full `ResolveTileYield`, including the neighbourhood scan) inside the sort comparator,
  i.e. O(n log n) resolves for n tiles; score each tile once into a pair vector and sort that.
- `WorkerAssignmentManager.cpp:348` — `availableTiles.erase(availableTiles.begin())` in a loop, and the
  tile is consumed even when `AssignWorker` returns false, silently skipping the pop.
- `WorkerAssignmentManager.cpp:16-19,298` — `IsUnassignedTile(pTile)` is a one-use alias for
  `pTile == nullptr`; `AutoAssignWorkers` then re-checks `pPop->IsWorker() && pPop->GetTile() == nullptr`
  (`.cpp:217`) on a vector `GetUnassignedWorkers_` just guaranteed those properties for.
- `WorkerAssignmentManager.cpp:39,44,72,142` — tiles are passed as nullable `const Tile*` everywhere and
  each function silently no-ops on null; guidelines prefer references, or a throw on an unexpected null.
- `ResourceManager.cpp:20-29` — `GetResourceValue_`'s `default: return 0` swallows a wrong `StatId_t`
  instead of throwing; the caller would then report modifiers as if they were production.
- `ResourceManager.cpp:38` — `if (!pUnit ...)` guards a container (`HomeBaseIndex::GetUnits`) that
  cannot hold nulls; either drop the check or throw.
- `ResourceManager.cpp:88-90,215-223` — empty out-of-line destructor, and `ProduceNutrients_` /
  `ProduceMinerals_` are single-line one-caller wrappers around `CalculateResource_`; the five
  `Consume*` bodies (`180-213`) are copy-paste of one another.
- `ResourceManager.cpp:165-177` — `GetEconProduction`/`GetLabsProduction`/`GetPsychProduction` each run a
  full `ComputeWorked_` pass plus a full `CalculateResource_(Energy, …)`; reading all five stats is five
  worked-tile passes. This is the memoization recorded as still-open follow-up under prior finding 1.1
  (`docs/code-review-findings.md:71-76`); `WorkedTileIndex::GetRevision()` is now available to key it on.

**Observed outside slice:**
- `src/game/faction/base/BaseManager.cpp:97-105` — `OnPopGained` triggers `AutoAssignWorkers`, but
  `OnPopLost` does not, so a tile freed by starvation stays unworked while fallback specialists exist.
- `src/ui/base/BaseView.cpp:189-196` — the tile-click toggle depends on the conflated predicate above;
  it will need updating with the fix.
- `docs/architecture/effects-system.md:356-357` — says `ResourceManager::ProduceResources()` "stores the
  effects" (they are passed per call now) and describes the base-level pass as adding "flat"
  contributions, which is the documented cover for the `AddPercent` defect above.
