# Package 17 — Faction services: treasury, social engineering, diplomacy, trade, visibility

Findings re-verified against the tree at `c41a45f`.

## Scope and split

Three `[H]` and six `[M]`, across five classes that share no code. Lands as **three commits**:

- **A — the treasury and the trade.** `EconomyManager` owns the resource and gains the rule;
  a trade validates its aggregate cost and applies atomically.
- **B — visibility.** Kill the per-event, whole-map, allocating recompute.
- **C — social engineering and diplomacy hygiene.** Starting policies to config, the rating map
  memoized, `TradeKind` collapsed onto the variant, the single pending-proposal slot.

Package 4 owns the constructor/null-policy half of these classes and has already landed; this
package owns their behaviour.

## Verified diagnoses — commit A

### [H] `EconomyManager` owns the treasury but offers no way to spend from it

`EconomyManager.cpp:12-15` — `AddEnergy` takes any signed amount and applies it unchecked. Spending
is spelled `AddEnergy(-cost)` at four sites (`ProbeActionExecutor.cpp:169`,
`UnitOrderExecutor.cpp:381`, `ProbeActionEffects.cpp:99`, `DiplomaticActionExecutor.cpp:314`) and
the affordability rule is re-implemented at three more (`ProbeActionExecutor.cpp:165`,
`TerraformRules.cpp:176`, `DiplomaticActionExecutor.cpp:227`).

Verified: every site does check today, so nothing is currently broken. The defect is that the
invariant lives in the callers. The fifth spender is the one that forgets.

**Chosen:** `SpendEnergy(int)` that throws on an amount that would go negative (and on a negative
amount — spending a negative is an `AddEnergy` in disguise), plus `CanAfford(int)` for the
callers that legitimately need to ask first. The three duplicated checks become `CanAfford`.

**Not done:** a bankruptcy rule. There is no game rule for a negative treasury, and inventing one
would be making up mechanics. Throwing is the correct placeholder: it converts a silent invariant
break into a loud one.

### [H] Trade items are validated one at a time and applied all at once

`DiplomaticActionExecutor.cpp:227` — `ValidateItem_` checks each `TradeCredits_t` against the
giver's *full* treasury, independently, against the unchanged pre-trade state.
`ApplyItems_` (`:288`) then debits every item.

Verified concretely: a giver with 60 energy and a proposal offering two `TradeCredits_t{50}` items
passes validation twice and applies both. The treasury ends at −40 with no error anywhere. The
same shape hits `TradeBase_t` (the same base offered twice) and every future consumable item.

**Chosen:** validation accumulates per giver. A `TradeCost_t` running total is built across both
directions of the proposal and checked against the giver's treasury once, and duplicate base ids
in one proposal are rejected. Application then goes through `SpendEnergy`, so the second line of
defence is the class that owns the resource — a half-applied trade now throws instead of silently
producing a negative balance.

**Rejected — a staged transaction that commits atomically.** It is the more complete answer, and
`TradeBase_t` in particular still leaves a half-applied deal if `TransferBaseTo` throws mid-loop.
But a real transaction means snapshot/rollback for base ownership, tech, ledger entries and
explored maps — a much larger change than this package, and one that wants the save-game
serialisation work to exist first. Validating the aggregate closes the reachable hole; the
residual (a throw from `TransferBaseTo` mid-apply) is recorded here rather than papered over.

### Architecture docs

`docs/architecture/economy-system.md` now records that `EconomyManager` owns the treasury and its
never-negative rule, not just the allocation split.

**Gap found, handed to package 16:** there is no architecture document for diplomacy or trade at
all — no diagram, no prose — though `.devin/rules/architecting.md` calls for a fine-grained diagram
per major subsystem. Trade semantics changed in this commit and there was nothing to update.
Writing that subsystem doc from scratch is package 16's remit (architecture docs sweep), so it is
recorded here as an input rather than improvised mid-package.

## Verified diagnoses — commit B

### [H] Visibility rebuild is a whole-map, per-event recompute

`FactionVisibleMap.cpp:104` — `RebuildFromSources` clears the map, re-reveals from every unit and
base, then **walks every tile of the world** and, for every improvement on every tile, calls
`SightRadiusFromImprovement_` (`:32`), which builds a `std::vector<ActiveEffect_t>` and runs
`ResolveStatModifiers` for each candidate effect.

That result depends only on `ImprovementConfig_t` — immutable registry data. All of it is
recomputed, with allocation, on every rebuild.

Trigger frequency, verified: `UnitManager` rebuilds in `CreateUnit` (`:66`), `DestroyUnit`
(`:117`), `ReleaseUnit` (`:138`) and `AdoptUnit` (`:152`); `DestroyUnit` recurses per passenger,
so sinking a loaded transport does one full-map scan *per unit aboard*. `GameState`'s
`OnUnitMoved` rebuilds on every move. Each rebuild then invokes `FirstContactResolver`, which
scans every other faction's units and bases.

**Chosen, two parts:**
1. `visionRadius` becomes a field on `ImprovementConfig_t`, computed once by the parser from the
   same effects it already reads. The rebuild then reads an int. This removes every allocation
   and every stat resolve from the hot path.
2. `Faction::VisibilityRebuildScope`, an RAII deferral mirroring `UnitManager`'s existing
   `DeferredDestructionScope`: while a scope is open, `RebuildVisibility` marks dirty; the
   outermost scope rebuilds once. `DestroyUnit` opens one around its cargo recursion, so sinking
   a transport is one rebuild rather than one per passenger.

**Not done, and why:** a world-level index of tiles carrying a vision improvement would remove the
remaining O(width × height) walk. Improvements mutate through two unrelated paths —
`TileEffectsContext::Add/RemoveImprovementWithEffects` for gameplay and `Tile::AddImprovement`
directly from world generation — and `Tile` holds no back-pointer to `WorldMap`, so maintaining an
index means a hook that three call sites must remember. That is precisely the "fixed it in one
place, left a second copy" shape this review keeps catching. The walk after this commit is
pointer-chasing with no allocation; the index is the right next step and wants a mutation choke
point built first.

### [M] Unsized visible map silently means "sees everything"

`UnitVisibility.cpp:97` — `if (rObserver.GetVisibleMap().IsSized() && !...IsVisible(rTile))`. An
unsized map skips the fog test entirely. `RebuildFromSources` has the mirrored no-op (`:81`).

Verified this is now dead-but-dangerous: `Faction`'s constructor sizes both maps unconditionally
from its injected `WorldMap&` (`Faction.cpp:70-71`), and the map is a reference, so no live
faction can have an unsized visible map. The guard therefore cannot fire — but if it ever did, it
would grant omniscience rather than fail.

**Chosen:** delete the guard in `UnitVisibility` and make `RebuildFromSources` throw on an unsized
map. Keeping a branch whose only behaviour is "see everything" is the compatibility shape the
guidelines say to delete.

## Verified diagnoses — commit C

### [M] Hardcoded default policy ids hard-fail a modded game

`SocialEngineeringManager.cpp:15-18` — four compiled-in string literals (`"frontier"`, `"simple"`,
`"survival"`, `"none_future"`), each looked up with `Get` (throws) in the constructor. A mod that
ships different starting policies does not get a nullptr any more; it gets a throw from every
faction constructor and a game that will not start.

**Chosen:** a `"default": true` flag per policy in `social_policies.json`, validated at load —
exactly one default per category, which is a config error worth failing on, and it fails at
config load with the offending category named rather than at faction construction.

### [M] `GetSocialRating` recollects and re-accumulates the whole map per query

`SocialEngineeringManager.cpp:89-94` — every call runs `CollectEffects()` (a fresh vector with a
`std::string` per effect) and `AccumulateSocialRatings` (a fresh `std::map`) to answer about *one*
axis. `SocialEngineeringDisplay` asks once per axis per frame — ten vector+map allocations per
frame.

**Chosen:** memoize the accumulated map against the `Revision` the class already owns and bumps in
`SetActivePolicy`. Same shape as the effects-pool memo from package 2.

### [M] `TradeKind` and its probe table duplicate the variant three times

`DiplomacyActions.h:23` and `DiplomacyActions.cpp:106` — adding an alternative to `TradeItem_t`
means editing the enum, the `probes` array, the parallel `kindOrder` array and `ToString`, with
nothing failing to compile if one is missed. The probe table also passes dummy payloads
(`TradeCommFrequency_t{0}`, `TradeBase_t{1}`) that are wrong answers the moment `CanTrade`
inspects a payload — an invariant stated only in a comment.

**Chosen:** derive the category list from the variant itself. One `TradeKindOf<T>` trait next to
the alternatives, and `GetAvailableTrades` iterates `std::variant_size` so a new alternative
without a trait fails to compile.

### [M] One global pending-proposal slot, silently overwritten

`DiplomaticActionExecutor.cpp:105` — `Propose` to a player-controlled faction assigns `m_pending`
unconditionally. A second proposal discards the first, whose proposer already received
`PendingPlayer` and will wait forever. `Accept` does not identify who is accepting.

**Chosen for this package:** reject a proposal while one is already pending
(`DiplomaticProposeResult::Busy`) rather than silently discarding it. A per-recipient queue is the
real answer, but there is no AI diplomacy driving this yet and the queue's semantics (ordering,
expiry, who may accept) are game rules that are not written down anywhere — inventing them here
would be making up mechanics. Refusing to lose a proposal is the part that is unambiguously
correct today.

## Review follow-ups applied

The multi-agent review of A+B+C could not run (it hit the account's monthly spend limit and
returned nothing). I reviewed the three commits myself against the failure modes that have bitten
this project repeatedly. One real finding, and several suspicions run down to nothing.

**`energy_cost` was never validated non-negative — and commit A turned that into a crash.**
`ImprovementConfigParser` accepted `"energy_cost": -5` verbatim. Before this package the terraform
path did `GetEnergy() >= -5` (always true) and then `AddEnergy(-(-5))`, so a negative cost was an
improvement that *paid* the player to build it — a silent config bug. Commit A routed that check
through `CanAfford`, which treats a negative cost as a caller bug and throws, so the same config
now crashes mid-order. That is exactly the "made something throw without tracing where the throw
lands" shape. Fixed at the source: the parser rejects a negative `energy_cost` at load, naming the
improvement, matching how `GrantEnergy` already rejects negative amounts. Two tests added, both
revert-verified.

**Run down and found sound:**
- Every remaining `AddEnergy` call is genuine income (probe steal credit, faction turn income, the
  receiving half of a trade, a council grant). `GrantEnergyEffect_t` is parse-validated `>= 0`, so
  a council grant cannot be a spend in disguise.
- `StealEnergyAmount_` clamps to the target's treasury read in the same call, with no mutation in
  between, so the `SpendEnergy` it feeds cannot overdraw. Integer overflow in `energy * pop` would
  produce a negative `share`, which both `max(0, ...)` terms drive to zero — still not an
  overdraw.
- `ProbeActionExecutor` returns early on a non-positive cost before reaching `CanAfford`, so probe
  actions cannot hit the negative-cost throw.
- Both `social_policies.json` files (shipped and fixture) carry the new `default` flag, and those
  are the only two files any `SocialPolicyRegistry::Load` call site reads.
- The rating memo's key is complete: only the constructor and `SetActivePolicy` write
  `m_activePolicies`, and the latter bumps the revision. The constructor does not, but
  `m_cachedRatingsRevision` starts at `UINT64_MAX`, so the first query cannot read an empty cache.
- `visionRadius` is derived in exactly one place. No production path builds an
  `ImprovementConfig_t` by hand; the two that do are tests unrelated to vision, where the `0`
  default is correct.
- `VisibilityRebuildScope` nests correctly through `DestroyUnit`'s self-recursion (each level
  holds its own scope; the outermost rebuilds once), and a rebuild requested from the
  first-contact handler during the final rebuild runs immediately rather than being lost — the
  dirty flag is cleared before the rebuild and the depth is already zero.
- Adding `DiplomaticProposeResult::Busy` breaks no switch: nothing outside the executor consumes
  that enum yet.

**Known and deliberate:** the scope's destructor performs the deferred rebuild, so anything that
rebuild throws terminates. `RebuildFromSources`'s new throw is unreachable (the constructor sizes
the map from a reference member), and swallowing in the destructor would leave a faction rendering
stale vision with no diagnostic. Noted at the destructor rather than papered over.
