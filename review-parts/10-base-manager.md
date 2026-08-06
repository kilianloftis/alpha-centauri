## Base management — BaseManager, home-base index, buildings

**Files:** `src/game/faction/base/BaseManager.cpp`, `include/game/faction/base/BaseManager.h`,
`include/game/faction/base/BaseTypes.h`, `src/game/faction/base/HomeBaseIndex.cpp`,
`include/game/faction/base/HomeBaseIndex.h`,
`src/game/faction/base/buildings/BuildingManager.cpp`,
`include/game/faction/base/buildings/BuildingManager.h`

**Assessment:** `BaseManager` is more than a pass-through: it owns the one genuinely
cross-subsystem job in the base (resolving the faction effect pool down to *this* base's
effect list) plus lifetime/identity, and the 2026-07 pass really did strip the population
and building pass-throughs. `HomeBaseIndex` is the strongest piece here — the claim *is*
the home-base link, so there is no unit-side field to desync, and destruction orphans
claims instead of dangling. The dominant weakness is that the base's effect list has **two
independent construction paths** that no longer produce the same list, and the memoized one
is the one the UI and half the turn pipeline read. Secondary: the constructor still accepts
ten nullable dependencies whose null behaviour is inconsistent (throw / silent skip /
deferred throw from a sub-manager).

### [H] Two divergent effect lists per base: the memo omits world and council effects
`src/game/faction/base/BaseManager.cpp:328` and `:343` — `ProduceResources` / `ApplyGrowth`
take a caller-supplied pool and build the base list fresh, while `ApplyProduction` (`:266`),
`GetMineralCost` (`:271`), the five production getters (`:194`–`:217`),
`GetNutrientsRequired` (`:348`) and `GetWorkedTileYield` (`:368`) use the memoized
`BuildBaseEffects_()` (`:300`) over `m_pEffectsProvider`'s pool alone. The supplied pool is
strictly larger: `Faction::ProduceBaseResources` / `ApplyBaseGrowth` (`src/game/Faction.cpp:455`,
`:469`) append `GameState::CollectWorldEffects` — other factions' `WorldGlobal` effects plus
the council's world laws and the governor's faction-global effects
(`src/game/GameState.cpp:130`–`140`). Concretely: `ResourceCollection` banks minerals
resolved *with* council effects while `BaseProduction` charges a mineral cost resolved
*without* them (`src/game/stages/BaseProduction.cpp:33`); `PopulationManager::ApplyGrowth`
spends a growth threshold computed with them
(`src/game/faction/base/population/PopulationManager.cpp:169`) while `GrowthDisplay` shows
the threshold from `GetNutrientsRequired()` computed without them. Same base, same turn,
two answers. Fix direction: make the provider's pool the only pool (fold world/council
contributions into `IEffectsProvider`) and delete the `FactionEffects_t`-taking overloads,
so `BuildBaseEffects_()` is the single entry point.

The memo itself is correctly *invalidated*, for what it covers: the key is the pool version,
and `FactionEffectsPool::CollectRevisions_` includes every per-base building and population
revision (`src/game/faction/FactionEffectsPool.cpp:117`–`129`), which are exactly the local
inputs of `CollectBaseLocalEffects_`. The bug is what never enters the key, not stale keying.

### [H] The constructor accepts ten nullable dependencies with three different null behaviours
`src/game/faction/base/BaseManager.cpp:48`–`64` — every registry, calculator, config and
manager arrives as a raw pointer that may be null, and the class then reacts differently to
each: `BuildBaseEffects_()` throws (`:302`), `m_pSocialRatings == nullptr` **silently skips
all social-rating expansion** (`:292`), a null pop-composition calculator makes
`RecalculateComposition` a no-op, and a null research/secret-project calculator surfaces as a
throw from inside `BuildingManager::GetBuildingsAvailableForConstruction`
(`src/game/faction/base/buildings/BuildingManager.cpp:71`) at whatever call site happens to
ask for the build list. `BuildingManager`'s own constructor repeats the pattern
(`BuildingManager.h:22`–`25`). This is not hypothetical harness-only slack: `tests/GameFixtures.h:115`
builds bases with a null rating registry and null secret-project calculator, so fixture bases
resolve social ratings to nothing while the real game resolves them — a behaviour difference
with no diagnostic. Prior finding 4.2 records this as fixed with "the one deliberately
nullable dependency left is `pEffectsProvider`"; the signature says otherwise, so the
recorded fix is incomplete. Fix direction: take references for everything that must exist
and let the one genuinely optional dependency stay a documented pointer.

### [M] `CollectBuildingEffects` re-implements the origin-tagging rule that the effects layer owns
`src/game/faction/base/BaseManager.cpp:233`–`241` stamps `originBase` for a hardcoded triple
(`ThisBase || ProducedAtThisBase || FactionUnits`), which is a copy of the rule already
implemented in terms of `LaneFor` at `src/game/effects/ActiveEffect.cpp:57`–`61`.
`BuildingManager::CollectEffects` passes `nullptr` for the origin
(`BuildingManager.cpp:63`) purely so this post-pass exists. Adding a new base-lane scope
updates `LaneFor` and silently misses `BaseManager`'s copy. Fix: let
`BuildingManager::CollectEffects` take the owning base and pass it to `AppendActiveEffects`,
then delete the loop.

### [M] `BaseSnapshot_t` carries an untyped production id that only round-trips for buildings
`include/game/faction/base/BaseManager.h:44`–`55` and `BaseManager.cpp:137`–`140` store the
current production as a bare `std::string`, and reconstruction resolves it exclusively
through the building registry (`src/game/Faction.cpp:300`–`309`). `UnitDesign` is already an
`IConstructable` (`include/game/units/UnitDesign.h:16`) and `GetConstructable()` is typed
`const IConstructable*`, so the first base that can queue a unit will throw out of
`BuildingRegistry::Get` the moment it is captured — the transfer path fails, not the unit
feature. The snapshot also silently drops `ResourceManager`'s accumulated per-turn stockpiles
(`ResourceManager.h:59`–`63`), which is undocumented either way. Fix: capture the item's kind
alongside its id (or a typed handle), and state in the struct comment what deliberately does
not survive a transfer.

### [M] `HomeBaseIndex` keeps three representations of one relation
`include/game/faction/base/HomeBaseIndex.h:78`–`79` holds `m_claims` and `m_units` as
index-parallel vectors, and each claim additionally stores `m_pUnit` (`:42`). `m_pUnit` is
written in five places (`HomeBaseIndex.cpp:12`, `:46`, `:52`, `:62`, `:68`) and **read nowhere** —
release and move both locate entries by claim address. Two of the three copies are dead
weight that a future maintainer must keep consistent by hand (the `m_units.begin() + idx`
arithmetic at `HomeBaseIndex.cpp:104` is the only thing keeping the arrays aligned). Fix:
delete `m_pUnit`; keep `m_units` since `GetUnits()` returns it by reference.

### [M] `HomeBaseIndex` throws invariant violations from `noexcept` paths
`src/game/faction/base/HomeBaseIndex.cpp:100` throws `std::logic_error` from `Release_`,
reached from `~HomeBaseClaim` (`:18`, implicitly `noexcept`), and `:113` throws from
`UpdateClaimPointer_`, reached from `MoveFrom_` via the explicitly `noexcept` move
constructor/assignment (`HomeBaseIndex.h:25`–`26`). Either throw is an unconditional
`std::terminate`, not the catchable error the code appears to promise. I could not construct
a call sequence that reaches them today (claims are only created by `Claim`, which registers
them, and C++17 guaranteed elision makes the returned object the registered one), so this is
a contract problem rather than a live defect. Fix: make them assertions, or handle the
missing entry, and stop implying an exception contract that cannot hold.

### [M] Instantaneous `Infiltration` is unreachable from the only production-completion path
`src/game/faction/base/BaseManager.cpp:113` calls `DispatchInstantaneousEffects(building, *this)`
with no `GameState`, and `BaseManager` has no route to one. The infiltration branch therefore
prints a `[TODO]` line to stderr and drops the effect
(`src/game/effects/ActiveEffect.cpp:428`–`437`) for every building a base ever completes —
this is the *only* production dispatch site in the game. No shipped building declares one yet,
so nothing is wrong today, but a modder who writes one gets a stderr note and no effect
rather than a rejected config. Fix direction: give the dispatch site the session it needs
(inject the world surface the base already needs for conquest, or route completion through a
handler that has `GameState`), or reject the effect kind at config validation.

### [L] Convention and hygiene items
- `include/game/faction/base/BaseManager.h:11` — `<functional>` is included but no
  `std::function` appears in the header.
- `include/game/faction/base/BaseManager.h:3` — `BuildingConfigParser.h` is included but no
  building config type is named in the header (`BuildingRegistry` is forward-declared);
  every consumer of a base pulls in the parser.
- `include/game/faction/base/BaseManager.h:40` — `using BaseId_t = int;` lives here while its
  sibling `FactionId_t` lives in `BaseTypes.h`, and the class then bypasses its own alias
  (`int m_baseId`, `int GetBaseId()` at `:190`).
- `include/game/faction/base/BaseTypes.h:12` and `:37` — `TradeRoute_t` and `TileCoord_t`
  have zero users anywhere in the tree; speculative types in a shared header.
- `include/game/faction/base/BaseTypes.h:19`–`24` — `TileResources_t`'s three ints have no
  default member initializers, so a default-constructed one is indeterminate.
- `src/game/faction/base/BaseManager.cpp:109`–`112` — dead null check: the preceding line's
  `AddBuilding` already throws on the same registry pointer.
- `src/game/faction/base/BaseManager.cpp:132` and
  `src/game/faction/base/buildings/BuildingManager.cpp:59` — null guards on a vector that
  cannot contain null (`AddBuilding` stores `&Registry::Get(...)`), while
  `DestroyBuilding`/`DoesBuildingExist_` dereference the same pointers unguarded.
- `src/game/faction/base/BaseManager.cpp:189`–`192` — `UserAssignBestAvailableWorker` is a
  one-line delegate to an already-public manager, contradicting the rule stated at
  `BaseManager.h:58`–`60` ("a method lives here only when it coordinates two or more
  subsystems").
- `src/game/faction/base/BaseManager.cpp:89` and `:120` — the `"Base"` improvement id is
  hardcoded in two places (prior review's recurring theme 2, magic config ids).
- `include/game/faction/base/BaseManager.h:206`,`:211` — two overloads named
  `BuildBaseEffects_` with materially different semantics (fresh vs memoized, supplied pool
  vs provider pool); the arity is the only cue at the call site.
- `src/game/faction/base/buildings/BuildingManager.cpp:20`–`22` — empty user-declared
  destructor (the class holds no incomplete types); it also suppresses implicit moves.

**Observed outside slice:**
- `src/game/Faction.cpp:455`,`:469` — the world-effect append that creates the two-list split
  above lives here; the base-side half is reported in this slice.
- `docs/architecture/effects-system.md:356` — states `ResourceManager::ProduceResources()`
  "stores the effects"; it takes them per call and stores nothing (`ResourceManager.h:50`).
