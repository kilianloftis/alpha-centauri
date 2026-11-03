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
  build and removal, which also lets a condenser's rerun aggrieve an AI through
  `RainfallGrievant`) and a switch.

### Wants-revenge terraform betrayal

- **Original:** a rainfall grievant that wants revenge on the terraformer (`DIPLO_WANT_REVENGE`)
  is marked to betray it (`SHALL_BETRAY`) instead of gaining +5 friction ("Integrity / friction
  writers", `action_terraform`).
- **Ours:** we keep no revenge record, so the grievant always gains
  `terraform_rainfall_friction`. TODO at `RainfallGrievant`.
- **Where:** `RainfallGrievant` (`TerraformRules`).
- **Missing:** a revenge record, a betrayal flag, and the branch on them.

### Refused-demand battle friction

- **Original:** a defender stronger than the owner of the attacker's tile, which is the
  attacker's Pact partner and refused the defender's `#DEMANDATTACK`, gains +3 friction toward
  that owner ("Integrity / friction writers", `battle_fight_2`).
- **Ours:** not written. TODO at `AlliedTerritoryOwner`.
- **Where:** `AlliedTerritoryOwner` (`DiplomaticTransitionRules`).
- **Missing:** talk's `#DEMANDATTACK` refusal and its flag, a military-strength metric, and the
  writer.

### Allied-territory friction on an unowned tile `[Bug]` `[Thinker]`

- **Original:** the allied −1 reads `Faction[-1]` when the defender's tile has no owner
  (no bounds check at `0x50A147`; "Integrity / friction writers", `battle_fight_2`).
- **Thinker:** adds `sq->owner >= 0` ("Added owner bounds checking").
- **Ours:** as Thinker: `AlliedTerritoryOwner` is nullopt for an unowned tile.
- **Where:** `AlliedTerritoryOwner` (`DiplomaticTransitionRules`).
- **Missing:** none playable: the original is an out-of-bounds read with no defined result, so
  this stays here only to record it.

### Allied-territory friction from naval and air combat `[Ours]`

- **Original:** the allied −1 is land only (`alt >= 3`), so a fight on an ally's ocean
  territory earns nothing, and neither does an air fight over it. A `CT_WEAPON_ONLY` single
  round between a sea and a non-sea unit that leaves both alive also returns first
  ("Integrity / friction writers", `battle_fight_2`).
- **Ours:** any tile in the owner's territory counts, land or water, so naval and air fights
  there earn the goodwill too, and so does every melee or duel that reaches the aftermath,
  whatever the units' domains. Defending an ally's coast is the same service as defending its
  land.
- **Where:** `AlliedTerritoryOwner` (`DiplomaticTransitionRules`);
  `allied_territory_battle_friction` in `config/diplomacy.json`.
- **Missing:** the original land-only version and a switch.

### Disengage is a chance roll for either side `[Ours]`

- **Original:** only a defender faster than its attacker withdraws, with one deterministic try
  once it has lost `(HP + 1) / 2` ("Disengage").
- **Ours:** either side may withdraw, rolling `disengage_chance` every round once it has lost
  half its starting HP.
- **Where:** `CombatResolver::TryDisengage_`, `DisengageRules::CanDisengage`,
  `PastDisengageThreshold`; `disengage_chance` in `config/unit_components/chassis.json`.
- **Missing:** the original version and a switch.

### Defender selection ignores unit plan and weapon `[Ours]`

- **Original:** `best_defender` ranks each unit by defense against this attacker, scaled by its
  HP fraction, minus its own weapon's offense, and multiplies the result by 16 for every combat
  plan, naval transport and terraformer. Colony pods, supply crawlers, probe teams and artifacts
  defend only when nothing else scores higher. Designated defenders and, in an artillery
  exchange, artillery get a flat bonus ("Defender selection").
- **Ours:** `SelectDefender` picks the best `DefenderWinChance` (strength × hits to kill), ties in
  tile order. A non-combatant whose odds are better than a combatant's defends first, and a
  stronger gun never keeps a unit out of the line.
- **Where:** `SelectDefender` (`AttackRules`).
- **Missing:** the original version and a switch.

### No agreement prompt before other hostile actions `[Ours]`

- **Original:** `break_treaty` with mask 7 (Pact, Treaty, Truce) also guards `sat_attack`,
  `action_airdrop`, `Console_terraform`, `Console_destroy`, `Console_use_ability` and
  `order_veh` ("`break_treaty`"). What each one asks about is not confirmed. Whether a bombard
  reaches the `battle_fight_2` prompt is not confirmed either.
- **Ours:** only an attack asks first (`StatusBrokenByAttack`), and a move into forbidden
  territory (`PresentTerritoryEntry_`). A bombard declares its sneak-attack Vendetta with no
  prompt.
- **Where:** `WorldView::TryBeginAttack_`, `WorldView::TryBeginBombard_`.
- **Missing:** confirm what each call site guards, then the prompts that are missing.

### Last-defender loss conditions `[Ours]`

- **Original:** killing a base's last defender costs one population only when that defender had
  a weapon and the attacker is not native life; an ocean square (the attacker's, a binary bug),
  an objective base at size 1, Perimeter Defense, Tachyon Field and difficulty 0 for a human
  also prevent it ("Last defender lost").
- **Thinker:** tests the base tile for ocean instead of the attacker's square (`Fix:`).
- **Ours:** any last-defender kill costs `LastDefenderPopLoss` (Perimeter Defense and Citizen
  difficulty clamp it to 0). A weaponless defender (colony pod, former, probe team) still costs
  population, and a native-life attacker applies the loss and then raids the base.
- **Where:** `ConquestOnLastDefenderKill` (`BaseConquestRules`).
- **Missing:** the original's weapon and native-attacker conditions, a Tachyon Field clamp, the
  ocean and objective checks, and a switch.

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

### Subvert Unit re-homes by Chebyshev distance

- **Original:** Subvert Unit re-homes to `base_find_2`'s nearest base of the actor ("Unit capture
  in combat"). `base_find_2` (`0x4E3C60`) measures like `base_find` (`0x4E3B80`): the binary's
  `vector_dist`, first base index on ties ("Integrity / friction writers", `action_terraform`).
- **Ours:** wrap-aware Chebyshev distance (`Faction::FindNearestBase`). Boarding already uses
  `GameState::FindNearestBase`, which matches `base_find`.
- **Where:** `SubvertCapture` (`CaptureRules`).
- **Missing:** the original version (`TabletopDiagonalDistance`, lower `BaseId` on ties) and a
  switch.

### Talk proposes a Truce at Vendetta `[Ours]`

- **Original:** the proposal menu's Treaty item is offered at Vendetta too, and
  `make_a_proposal` sends it to `propose_treaty`, whose `make_treaty` sets a Treaty and clears
  the Vendetta. Blood Truce is pledged only from an AI's own offer in `communicate` ("Talk").
- **Ours:** `StepUp(Vendetta)` is Truce, so talk offers a Truce there on that item and the AI
  answers `MAKETRUCE`; its refusal reuses `REJTREATY`.
- **Where:** `StepUp` (`DiplomaticTransitionRules`), `config/ui/talk.json`.
- **Missing:** the original transition and a switch.

### Trade proposals bundle items `[Ours]`

- **Original:** each proposal-menu line asks for or offers one thing, and the AI names a price
  or counter-offer (`COUNTER0`, `COUNTERMENU`) ("Talk").
- **Ours:** the player builds both sides of one proposal from `TradeKinds()`, and the AI
  accepts (`EvaluateResponse_`). A proposal that only gives values each item as its own
  `make_gift`, in order ("Gifts"). The player picks any tech, where the AI names one or two;
  there is no "all research data"; credit amounts come from `credit_offers`, where `make_gift`
  offers the giver's credits rounded to 25 and halving.
- **Where:** `config/ui/talk.json` trade scenes, `TalkConversation`, `ApplyProposal`.
- **Missing:** the one-item flow with AI counter-offers, which needs AI evaluation, and a switch
  (a variant `talk.json` here).

### Talk trades write no standing

- **Original:** `tech_trade` moves friction after a swap (`0x542034`, "Gifts").
- **Ours:** a trade that asks something back moves the items only. TODO at `ApplyProposal`.
- **Where:** `ApplyProposal` (`DiplomaticProposalMutations`).
- **Missing:** the RE of that delta, and the writer.

### Base gifts skip patience

- **Original:** the recipient's `diplo_patience` toward the giver drops by 2, or 3 when generous
  ("Base gift").
- **Ours:** friction, size and the gift count follow the binary, but there is no patience
  record.
- **Where:** `GiftStanding` (`DiplomaticGiftRules`).
- **Missing:** a patience record.

### Gift count ignores the great satan

- **Original:** a gift's second count step also passes the giver's blemishes when
  `great_satan(giver, 0)` holds or in multiplayer ("Gifts").
- **Ours:** always capped at the giver's blemishes.
- **Where:** `DiplomacyCalculator::NextGifts`.
- **Missing:** rankings, score and `great_satan`, and multiplayer.

### Generous gifts

- **Original:** a generous gift, base gifts included, clears revenge both ways (`treaty_off`
  `0x60`). Energy and tech reply `GAVEENERGY1` / `GAVETECH1`, or `…2` ("Gifts").
- **Ours:** there is no revenge record, and the reply is `talk.json`'s trade acceptance.
- **Where:** `ApplyProposal`, `config/ui/talk.json`.
- **Missing:** the revenge record and its clearing, a gift result reported to talk, and the
  `GAVE*` lines.

### Ranked greeting

- **Original:** `INTRONEW3` names the player's empire with a rank word indexed by
  `7 - ranking` ("Talk", Greeting).
- **Ours:** there is no faction ranking, so `talk.json` leaves `INTRONEW3` out and the greeting
  picks from the other five.
- **Where:** `config/ui/talk.json` `start`.
- **Missing:** a ranking, a value source for the word, and the variant.

### Submissive caption

- **Original:** the talk caption reads label 488 `Submissive` instead of the mood when the band
  is 0 and `0x93FA98` is set without a Pact, or when the rival has surrendered under a Pact
  ("`diplomacy_caption`").
- **Ours:** always the mood; there is no surrender state. TODO at `TalkBindings::Caption`.
- **Where:** `TalkBindings::Caption`.
- **Missing:** `0x93FA98` is `DiploFearScale` ("Base value"), filled by `diplomacy_check`, which
  is not mapped; a surrender state; and the label.

### Treaty acceptance caption `[Bug]`

- **Original:** `make_treaty` writes the speaker's pet project into slot 7 before
  `MAKETREATY`, whose `#caption $CAPTION7` then reads it, so the popup's caption shows the
  project, not the faction and mood ("Talk", Replies).
- **Ours:** the talk popup always draws the caption binding.
- **Where:** `TalkPopup::DrawCaption_`.
- **Missing:** the original version and a switch.

### Base swap accepts the worse trade `[Bug]` `[Thinker]`

- **Original:** a base for a base swaps when the asked base's value plus a roll beats the
  offered base's, and otherwise replies `NOBASESWAP3`, which calls the asked base the better one
  ("Base swap"). The AI gives up what its own valuation ranks higher.
- **Thinker:** `mod_base_swap` swaps only when the offered value is at least the asked one, with
  its own `base_trade_value`.
- **Ours:** the AI accepts every trade (`EvaluateResponse_`); see "Trade proposals bundle items".
- **Where:** `DiplomaticActionExecutor::EvaluateResponse_`.
- **Missing:** AI evaluation of base trades, all three versions, and a switch.

### Base gift reply ignores the gift's size `[Bug]`

- **Original:** `make_gift` builds `BASEGIFT1` or `BASEGIFT2` for a generous base gift but pops
  plain `BASEGIFT` ("Base gift"), so those two replies never show. The tech branch pops its
  built label.
- **Ours:** the reply is `talk.json`'s trade acceptance.
- **Where:** `config/ui/talk.json`.
- **Missing:** a gift result reported to talk, the `BASEGIFT` lines in both versions, and a
  switch.

### Region cap empties the rest of the map `[Bug]`

- **Original:** at the 62nd land or water region, `Path_continents` looks for the smallest region
  to push into overflow starting at the unassigned id 0 or 64, whose size is always 0. Every tile
  not yet reached, of both surfaces, becomes overflow ("Regions").
- **Ours:** no cap: every land and water body keeps its own region.
- **Where:** `AssignRegions` (`RegionRules`).
- **Missing:** the original version (the 62-per-surface cap with its empty-slot search) and a
  switch.

### Base value skips the AI's attitude and scenario inputs

- **Original:** `value_of_base` halves for the AI's `fear` and quadruples for its `hostile`
  flag, both filled by `diplomacy_check`. It quadruples for a scenario objective and doubles
  for a `BELIEVE` evaluator ("Base value").
- **Ours:** none applies: there is no attitude model, there are no scenarios, and the Believers
  do not ship. On a base gift the evaluator is the recipient, so the Believers' doubling there
  is their `gift_received_value` weight on `Base` ("Gift weights" in
  `docs/architecture/diplomacy-system.md`).
- **Where:** `BaseValue` (`BaseValueRules`).
- **Missing:** the RE of `diplomacy_check`'s fear, hostility and attack chance, and those
  inputs; scenario objectives; a faction-scope base value stat with the Believers, which base
  swaps and threatened cession need and which then takes the doubling over from the gift
  weight.

### Poles are separate regions `[Ours]`

- **Original:** `Path_continents` sets every tile on the first and last rows to 63 (land) or 127
  (water), so the north and south poles share a region per surface ("Regions").
- **Ours:** each pole takes its own id per surface, so a base on one pole does not count the
  other pole's bases as in its region.
- **Where:** `AssignRegions` (`RegionRules`).
- **Missing:** the original version and a switch.

### Region base counts are live `[Ours]`

- **Original:** `region_total_bases` is recounted at each faction's upkeep and moved by
  `base_init` / `base_kill`, so a captured or ceded base counts for its old owner until then
  ("Regions").
- **Ours:** counted from current ownership on every query.
- **Where:** `BaseValue`'s region counts (`BaseValueRules`).
- **Missing:** the original version and a switch.

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

- **Terraform rainfall friction** ("Integrity / friction writers", `action_terraform`):
  Thinker's `action_terraform` rewrite drops both the +5 and the wants-revenge betrayal.
- **A contacted faction with no agreement is asked "Pronounce Vendetta!"** ("`break_treaty`"):
  Thinker's `battle_fight_2` passes Truce instead of Commlink (`Fix:`), so it attacks with no
  prompt.

## Variant files

Files in this directory are not loaded by default. Each one is a non-default version of a
rule from an entry above.

| File | Kind | Version of | Entry |
|---|---|---|---|
| [`tech_cost_thinker.lua`](tech_cost_thinker.lua) | Thinker | `config/tech_cost.lua` | [Tech cost formula](#tech-cost-formula-ours-thinker) |
| [`creche_morale_manual.json`](creche_morale_manual.json) | Manual | Children's Crèche and Brood Pit effects | [Children's Crèche as the manual describes it](#childrens-crèche-as-the-manual-describes-it-manual) |
