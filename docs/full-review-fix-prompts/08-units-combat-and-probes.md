# Package 8 — Units: combat, probes, conquest, morale

**Source package:** [`docs/full-review-fix-packages.md`](../full-review-fix-packages.md), Package 8
**Verified against:** working tree at commit `e1e7aa1` (after full-review Packages 1–7)

**Status: complete** for the findings this package owns. Both `[H]`s and all five `[M]`s fixed;
the `[L]` hygiene block is batched into package 16.

---

## Done

### [H] Roll `DisengageChance` before committing a withdrawal — FIXED

`TryDisengage_` moved the unit as soon as eligibility, the half-HP gate and a retreat tile
existed. It never read `StatId_t::DisengageChance` — a stat that is defined, shipped on the
Speeder chassis (`disengage_chance: 25`) and documented as a percent. Every eligible unit
therefore withdrew, every time.

The roll now sits after the half-HP gate and *before* `MoveUnit`, so it gates the state change
rather than following it.

Existing disengage tests were relying on the missing roll: they exercise *eligibility* (half HP,
retreat tiles, ZOC, Comm Jammer, terrain), so both sides now carry a `test_always_disengages`
fixture component (`disengage_chance: 100`) to keep them deterministic. A new test covers the
roll itself with a 0%/100% pair.

### [H] Keep the originating base on building intercept candidates — FIXED

ThisBase intercepts are collected from a specific `BaseManager`, but `InterceptCandidate_t` only
stored `sourceId`. On fail-destruction, `MaybeDestroyInterceptSourceOnFail_` re-derived the base
with `Faction::FindBaseWithBuilding`, which returns the *first* base owning that id — so with the
same building in two bases the wrong copy was destroyed and `NotifyBuildingDestroyed` cleared a
deploy against the wrong inventory.

The candidate now carries `pBaseSource`, set on the ThisBase lane. `FindBaseWithBuilding` remains
the fallback for FactionGlobal / AllOwnerBases charges, which genuinely belong to no single base.

### [M] Probe sabotage skips deploy-ledger notification and conflates facility vs random — FIXED

Three separate defects:
- `DestroyBuilding` was called without `Faction::NotifyBuildingDestroyed`, unlike every other
  destruction path (raze, orbital attack, intercept fail), so a sabotaged ODP left
  `m_buildingDeploys` stale and `CountReadyBuildings` under-counted the faction's *surviving*
  copies for the rest of the cooldown. Now routed through `DestroyBuildingAndNotify_`.
- `SabotageFacility` with an empty `facilityId` fell through to the random branch — a different
  action.
- A non-empty but missing id still reported `ProbeDestroyedFacility_t` after `DestroyBuilding`'s
  documented no-op, claiming a kill that never happened. Targeted sabotage now requires the
  building to be present and returns a `NoTarget` failure otherwise.

### [M] Intercept condition context marks the wrong combat role — FIXED

Candidates were filtered with `CombatRole_t::Attacker` and a null `pAttacker`, so an
`IsDefending` condition on an intercept effect never matched and `AttackerIsEmbarked` was always
false — silently, even though `UnitFilterSatisfied` on the next line already had the attacker.
Now `CombatRole_t::Defender` with `pAttacker` set.

### [M] Unit-subvert cost treats HQ-tile distance 0 as the no-HQ default — FIXED

`DistanceToHeadquarters_` already substitutes `k_defaultHqDistance` when there is *no* HQ, so a
distance of 0 means "on the HQ tile" — a real distance. `QuoteSubvertUnitCost_` mapped it back to
12 anyway, so subverting the HQ garrison used denominator `12 + distBias` instead of being
refused: the best-defended tile on the map was the cheapest to subvert. Now returns `nullopt`,
matching `QuoteMindControlBaseCost_` on the same input.

### [M] Escape-pod design failures fail closed without error — FIXED

`EnsureEscapePodDesign_` returned `nullptr` when the component registry was missing or a
configured component id was unknown, and `SpawnEscapePods_` then returned 0 — while
cross-species capture had already stripped population. The player lost the pops the rule says
they escape with, silently. Both cases now throw naming the problem. An empty `componentIds`
stays a legitimate "this ruleset has no escape pods".

### [M] `risk_repeat` depends on a caller flag the executor never owns — FIXED

Confirmed exactly as described: `TryProbeAction` took `bool bRepeatAtBase = false` and **no
caller anywhere passed it**, so the config field `risk_repeat` was parsed, stored, and never
reachable — dead config.

Whether an attempt is a repeat is session history, so the executor now owns it: it records
(actor faction, target base) pairs and derives the flag itself. The parameter is gone. The
attempt is recorded *before* the roll, so a failed probe still counts — it tips the base off
either way.

**TODO left in place:** the *scope* of "repeat" is not specified by any rule available here.
This treats it as permanent per (faction, base) for the session; whether it should decay over
turns or reset when the base changes hands needs the SMAC rule
(`.devin/rules/coding-guidelines.md`: do not invent mechanics).

---

## Not in scope here

- **Probe sabotage destroying a Secret Project without a tombstone** — package 9 owns the single
  tombstone rule for all destruction paths; this package owns only the ledger/targeting half, as
  the package text says.
- **`[L]` hygiene block of the units combat section** — batched into package 16's codebase-wide
  sweep so the directory moves at once.
