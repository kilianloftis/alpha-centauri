# Deviations from the original game

Every place this game's mechanics or rules differ from Alpha Centauri (`terranx.exe`, Alien
Crossfire v2.0), and every variant file that would make it differ.

[`docs/thinker/terranx-map.md`](../../docs/thinker/terranx-map.md) records what the binary does
and where Thinker differs. This file records what **we** do about it. An entry cites the map
section for the original behaviour instead of repeating the disassembly.

## Kinds

- **Bug**: the binary contradicts its own display, text, or evident intent.
- **Thinker**: Thinker changes the binary, marked `Fix:` or behind a config option.
- **Manual**: the manual or wiki describes a rule the binary does not implement.
- **Ours**: a rule we chose that neither the binary nor Thinker has.

## Entry format

```markdown
### Short name `[Kind]`

- **Original:** what the binary does, with the terranx-map.md section.
- **Thinker:** what Thinker does, when it differs.
- **Ours:** what this game does, and why.
- **Where:** the Rules function, config, or variant file that carries it.
- **Missing:** what still stands between the entry and Completed (TODO entries only).
```

## When an entry is Completed

An entry is Completed only when every version of the rule (the original, Thinker's where it
differs, and ours) is playable:

- **Code deviations:** each version is implemented, and a config switch selects between them.
  The switch follows `code-kinds.mdc`: a field on the subsystem's config struct, read by the
  Rules function that makes the decision, one function per version.
- **Effects, config, or Lua deviations:** each version is implemented as a file, and the
  versions not shipped by default are stored in this directory and listed under
  [Variant files](#variant-files). Swapping the file is the switch until
  [variant profiles](#variant-profiles) exist.

Until then the entry stays under TODO, with **Missing** saying which version or switch is
absent.

## TODO

### Probe display: HSA halves survival, not success twice `[Bug]` `[Thinker]`

- **Original:** `success_rates` halves success for HSA, then halves success **again** where
  survival was meant (`0x59F0B4`–`0x59F0BF`, "Displayed odds").
- **Thinker:** `mod_success_rates` applies the second `/2` to survival (`Fix:`).
- **Ours:** as Thinker. Display only; the rolls use the strength draws.
- **Where:** `ComputeProbeChances` multiplies survival by the target's `probe_success_scale`.
- **Missing:** the original display version and a switch.

### HSA bust coin on every escape `[Bug]`

- **Original:** after a successful escape, HSA's 50% bust coin runs only for a sea probe whose
  return base is not a port; the coin sits inside the port search (`0x5A4427`, "Escape roll").
  Thinker matches.
- **Ours:** the coin runs on every successful escape, as the displayed survival halving
  says.
- **Where:** `RollProbeAction` in `ProbeActionMutations.cpp` rolls the target's
  `probe_success_bust` after both successful draws.
- **Missing:** the original sea/no-port version and a switch.

### Combat odds dialog uses hits-to-kill `[Bug]` `[Thinker]`

- **Original:** the confirm dialog compares `HP × strength`, which is wrong when firepower is
  the receiver's reactor: fusion looks twice as tough as fission ("Combat odds").
- **Thinker:** `combat_odds_fix` uses strength × hits-to-kill for conventional fights, and
  skips psi.
- **Ours:** the hits-to-kill correction applies whenever a lost round costs `psi_damage`,
  psi included.
- **Where:** `CombatRules.h` odds ratio.
- **Missing:** the original and Thinker versions and a switch.

### Assimilation claims per previous owner `[Ours]`

- **Original:** one `assimilation_turns_left` counter and one `faction_id_former` per base
  ("Counter decay").
- **Ours:** the base keeps the occupier's drone window plus a claim for every faction that
  lost it; claims expire independently. Recapture by a faction with an active claim inverts
  that claim's elapsed time (12 turns away → 1 drone for 12 turns), even after a third party
  held the base. A later occupier still gets a fresh window.
- **Where:** `AssimilationTracker`, `AssimilationOnCapture` in `PopulationRules.cpp`;
  described in [`unit-movement-system.md`](../../docs/architecture/unit-movement-system.md).
- **Missing:** the original single-counter version and a switch.

### Engine bindings for `tech_cost_thinker.lua`

- **Missing:** `level` (prerequisite depth of the target tech) and `commlink_owners` as Lua
  inputs, and pricing each tech when it is selected rather than once per breakthrough. Until
  then the file cannot be swapped in.

### Variant profiles

- **Missing:** a profile setting that loads a [variant file](#variant-files) in place of the
  shipping one, so swapping does not mean editing `config/`.

### Predefined unit plans

- **Original:** predefined `alpha.txt` units state their plan; Scout Patrol (infantry, gun,
  no armour) is `PLAN_RECON`, where `make_proto` would derive `PLAN_OFFENSE` for the same
  parts ("Unit plan"). Recon does not count toward the mind-control garrison.
- **Ours:** there are no predefined designs, so every design's plan comes from
  `UnitPlanRules::DesignPlan`, and a Scout Patrol equivalent counts as offense.
- **Missing:** predefined designs with an explicit plan that overrides `DesignPlan`.

### Wants-revenge mind-control surcharge

- **Original:** a base whose owner was not an atrocity victim of the actor but wants revenge
  on it (`DIPLO_WANT_REVENGE`) costs `cost + cost / 2` to mind-control ("Mind-control cost").
- **Ours:** we keep no revenge record, so the surcharge never applies. TODO at
  `QuoteBaseActionCost_`.
- **Missing:** a revenge record, a condition on it, and a ×1.5 `probe_actor_cost` effect.

### Drain-energy repeat risk `[Bug]`

- **Original:** `probe` tests `BSTATE_ENERGY_RESERVES_DRAINED` for a risk-1 repeat but never
  sets the bit, so the repeat bump is dead ("Probe: action ids, HSA, odds, and the roll").
- **Ours:** undecided. `MissionRisk` applies `risk_repeat` to steal tech only, as the binary
  does.
- **Missing:** a decision, then the repeat-risk version and a switch.

### Frame against a victim with no agreement `[Bug]`

- **Original:** the frame menu and its +1 risk ignore actor–target standing, but the
  aftermath returns at `has_treaty(actor, target, 0x7)` (`0x5A4B2F`) before either frame
  branch. Framing a target the actor holds no Truce, Treaty or Pact with costs risk and
  writes nothing: no target–framed Vendetta on success, no framed–actor Vendetta on a flop
  ("Frame"). Thinker matches.
- **Ours:** every frame resolves, whatever the actor and target hold. A held frame against a
  target with no agreement also skips the aftermath, so the target does not declare Vendetta
  on the actor.
- **Where:** `ResolveProbeFrame` in `ProbeRules.cpp`.
- **Missing:** the original agreement-gated version and a switch.

### Audit unclassified Thinker fixes `[Thinker]`

- **Original:** terranx-map.md records these Thinker `Fix:` changes without an entry here:
  - Colony pod at size 1: Thinker shows `ABANDONBASE1` (turns until growth) where the binary
    only has `ABANDONBASE` (`base_production`).
  - Combat morale: Thinker's combat path skips natives for the in-square Crèche/Brood Pit
    extras and adds HQ +1; `mod_say_morale2` adds the HQ `+` too ("Unit morale").
  - Sea level change: Thinker's `alt_set` clamps the reduced pop at 0 before testing it
    ("Sea bases").
- **Missing:** an entry for each, or a line under
  [Thinker changes not adopted](#thinker-changes-not-adopted).

### Condenser moisture is a local overlay `[Ours]`

- **Original:** a condenser is an input to the full `world_rainfall` scan. It adds up to +2 on
  its 3×3, sets `rain_flag` (which carries east along the row), and its wet tiles feed the
  arid-neighbour smoothing pass ("Climate", `world_rainfall`).
- **Ours:** a radius-1 `moisture_tier` +1 `StatModifier` on top of the world-gen base moisture.
  There is no eastward carry and no smoothing spill.
- **Where:** `Condenser` in `config/improvements.json`; `TileEffectsContext::RecomputeMoisture`.
- **Missing:** the original version (a condenser as a `RecomputeRainfall` input, re-run on
  build and removal) and a switch.

### Disengage is a chance roll for either side `[Ours]`

- **Original:** only a defender faster than its attacker withdraws, with one deterministic try
  once it has lost `(HP + 1) / 2` ("Disengage").
- **Ours:** either side may withdraw, rolling `disengage_chance` every round once it has lost
  half its starting HP.
- **Where:** `CombatResolver::TryDisengage_`, `DisengageRules::CanDisengage`,
  `PastDisengageThreshold`; `disengage_chance` in `config/unit_components/chassis.json`.
- **Missing:** the original version and a switch.

### Collateral hits colony pods, probes and supply crawlers `[Ours]`

- **Original:** collateral skips non-defender occupants with plan colony, probe, supply or
  artifact ("Unit capture in combat", Alien artifacts).
- **Ours:** only the artifact is shielded, by its own `collateral_susceptibility` × 0. Colony
  pods, probe teams and supply crawlers take the splash.
- **Where:** `PlanStackCollateral`; `collateral_susceptibility` on each design.
- **Missing:** the original version (susceptibility 0 on those designs) and a switch.

### Artifact capture sets no want-to-talk `[Ours]`

- **Original:** taking an AI faction's artifacts outside Vendetta adds friction 5 and flags the
  victim as wanting to talk ("Unit capture in combat", Alien artifacts).
- **Ours:** the friction only; there is no want-to-talk state.
- **Where:** `ArtifactCaptureFriction` (`CaptureRules`).
- **Missing:** want-to-talk state, and the original version behind a switch.

### Boarded units lose their orders `[Ours]`

- **Original:** a boarded unit keeps its order; `veh_skip` only spends its moves ("Marine
  Detachment", Capture).
- **Ours:** every captured unit's order is cleared, so it never carries out the old owner's.
- **Where:** `CaptureUnits` (`CaptureMutations`).
- **Missing:** the original version and a switch.

### Configurable home base for captured units `[Ours]`

- **Original:** boarding homes each unit at the nearest base of any owner, kept only when the
  captor owns it with population ≥ 3; Subvert Unit homes it at the captor's nearest base; an
  artifact taken on defeat gets none ("Unit capture in combat").
- **Ours:** planned: each capture kind (boarding, Subvert Unit, taken on defeat) takes its
  home-base rule from config: which bases to search (any owner, the captor's, or none) and a
  minimum population, with the original as the default. Today only boarding's minimum is
  config (`boarding_home_base_min_pop`); the searches are fixed in code.
- **Where:** `BoardingCapture`, `SubvertCapture`, `DefeatedUnitCapture` (`CaptureRules`);
  `config/combat.json`.
- **Missing:** the per-kind search setting and minimum population in `combat.json`, read by
  those rules.

### Boarding captures only the defender `[Ours]`

- **Original:** a successful boarding takes every unit on the defender's tile, cargo and any
  ally's ships included ("Marine Detachment", Capture).
- **Ours:** planned: a config switch so a boarding takes only the defender and the cargo it
  carries, leaving the rest of the stack to its owner. The original whole-tile capture stays
  the default.
- **Where:** `BoardingCapture` (`CaptureRules`).
- **Missing:** the switch in `config/combat.json`, read by `BoardingCapture`, and the
  defender-only version. That version must settle the captured ship sharing a tile with its
  former owner's units, which the stacking rules do not allow between enemies.

### Nearest base by Chebyshev distance

- **Original:** boarding re-homes to `base_find`'s nearest base and Subvert Unit to
  `base_find_2`'s ("Unit capture in combat"); their distance measure is not mapped.
- **Ours:** wrap-aware Chebyshev distance (`Faction::FindNearestBase`).
- **Where:** `BoardingCapture` and `SubvertCapture` (`CaptureRules`).
- **Missing:** confirm `base_find`'s distance in the binary; if it differs, the original version
  and a switch.

### Sweep existing docs for unlisted deviations

- **Missing:** read `docs/architecture/` and
  [`docs/mechanic-decisions-review.md`](../../docs/mechanic-decisions-review.md) ("Outside the
  plan") for behaviour that differs from the binary, and add an entry for each.

## Completed

### Tech cost formula `[Ours]` `[Thinker]`

- **Original:** `tech_rate` (`0x5BE6B0`, "Research").
- **Thinker:** `revised_tech_cost` / `tech_alt_cost` prices each tech by prerequisite depth and
  commlink owners.
- **Ours:** shipping [`tech_cost.lua`](../tech_cost.lua) follows `tech_rate` through the map
  step, except MOSTTECHS is taken over every faction in the session and TECHS is
  `max(1, discovered)` with no 9999 ranking cap.
- **Where:** `config/tech_cost.lua` (default), [`tech_cost_thinker.lua`](tech_cost_thinker.lua).

### Children's Crèche as the manual describes it `[Manual]`

- **Original:** Crèche halves negative SE Morale for units whose home base has it, and adds
  in-square extras ("Unit morale").
- **Manual:** units in the base are at least rank 1.
- **Where:** `config/buildings/buildings.json` (default, original),
  [`creche_morale_manual.json`](creche_morale_manual.json) (Crèche and Brood Pit effect lists
  keyed by building id).

## Thinker changes not adopted

We play the binary's version of these, and Thinker's version is not implemented. Implementing
it behind a switch turns the line into a TODO entry.

- **Home Crèche halves negative SE Morale** ("Unit morale"): Thinker's `mod_morale_veh`
  removes the half (`Fix:`).
- **SE Probe ≥ 3 is immune to subversion, even with Algorithmic Enhancement** ("Mind-control
  cost"): Thinker's Fix 1 & 15 lets Algo through at ×2.
- **Conventional rounds deal 1 damage** ("Combat odds"): Thinker's `ignore_reactor_power`
  (default on) uses reactor firepower.
- **Drain energy amount** ("Drain energy"): Thinker multiplies by `conf.steal_energy_rate`.
- **A flopped frame still runs keep-or-break** ("Frame"): Thinker's `mod_probe` flips
  `prb_state` positive after the framed faction's Vendetta, so the flop also writes the
  successful frame's Vendetta and skips keep-or-break.

## Variant files

Files in this directory are not loaded by default. Each one is a non-default version of a
rule from an entry above.

| File | Kind | Version of | Entry |
|---|---|---|---|
| [`tech_cost_thinker.lua`](tech_cost_thinker.lua) | Thinker | `config/tech_cost.lua` | [Tech cost formula](#tech-cost-formula-ours-thinker) |
| [`creche_morale_manual.json`](creche_morale_manual.json) | Manual | Children's Crèche and Brood Pit effects | [Children's Crèche as the manual describes it](#childrens-crèche-as-the-manual-describes-it-manual) |
