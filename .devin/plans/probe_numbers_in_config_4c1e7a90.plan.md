---
name: Probe numbers in config
overview: Move every probe cost, risk, drain and plague factor out of C++ into config, put the Pact cost modifier in diplomacy.json as a FactionPair effect, move the calculations probe mutations still make into ProbeRules, and match the binary on which quotes the relation modifiers reach.
todos:
  - id: pact-pair-stat
    content: MindControlPartnerScale (Faction, PureMultiplier); diplomacy.json Pact emits FactionPair x2; base quote resolves it on the actor toward the target
    status: completed
  - id: base-relation-config
    content: ProbeCostConfig_t former_owner_scale and atrocity_victim_scale (base targets); riot and golden age as MindControlScale mood effects; applied in binary order, truncating; unit quotes get none
    status: completed
  - id: action-numbers
    content: sabotage_facility protected_risk; drain_energy floor_morale_divisor and remainder_divisor; genetic_plague repeat_divisor and garrison_damage_divisor
    status: completed
  - id: mutations-to-rules
    content: Plague garrison damage bound and amount, mission and escape draw outcomes move to ProbeRules; mutations only draw
    status: completed
  - id: tests-docs
    content: Fixture-driven tests for every factor; terranx-map, effects-system, probe docs, deviations TODO for revenge; ./bd build and ./bd test
    status: completed
isProject: false
---

# Probe numbers in config

## Original mechanics

Confirmed in [docs/thinker/terranx-map.md](docs/thinker/terranx-map.md) ("Mind-control cost",
"Unit subversion cost") and Thinker `probe.cpp`:

- **Base mind control** (`mind_control` `0x59EA80`), after the formula and the AI-vs-human
  discount, in order, each an integer step: Pact ×2 (`0x59EDC2`), former owner /2, drone riot
  /2, golden age ×2, atrocity victim ×2, else wants-revenge ×1.5 (`cost + cost / 2`).
- **Unit subversion** (`0x5A14BF`–`0x5A1800`) reads no diplomatic status and no atrocity
  record: none of those modifiers apply.

## Pact: diplomacy.json

- New stat `MindControlPartnerScale` (`mind_control_partner_scale`; PureMultiplier, Faction
  domain): what a faction pays to mind-control a partner's base, from their diplomatic status.
- [diplomacy.json](config/diplomacy.json) `Pact` gains a `FactionPair` `StatModifier`
  `mind_control_partner_scale` `MultiplyGeometric` 2, beside its `commerce_rate`. Same in the
  diplomacy fixture.
- The base quote resolves it with `ResolveFactionStat` on the actor's effects, `ctx.pFaction` the
  actor and `ctx.pPartner` the target faction, as `CommerceRateToward_` does.

## Riot and golden age: the base's mood effects

[pop_composition.json](config/pop_composition.json) already emits these: each riot tier and the
golden age effects. They move from `probe_action_cost` `AddPercent` (−50 / +100) to a new stat,
`MindControlScale` (`mind_control_scale`; PureMultiplier, Base), `MultiplyGeometric` 0.5 / 2:

- `probe_action_cost` also prices unit subversion through the home base, which the binary's unit
  path never reads;
- as `AddPercent` they summed with the SE Probe table instead of multiplying.

Same in the `pop_composition.json` fixtures.

## Other base relation modifiers: probe_actions.json

[probe_actions.json](config/probe_actions.json) base-target costs (`mind_control_base`,
`total_thought_control`) gain required numbers, rejected on unit targets as
`mind_control_divisor` is:

```json
"former_owner_scale": 0.5,
"atrocity_victim_scale": 2
```

`ApplyDiplomaticCostMods_` becomes `ApplyBaseRelationMods_(cost, rCost, rActor, rBase)`, applied
to the base formula before the cost multiplier:

1. × `mind_control_partner_scale` toward the target.
2. × `former_owner_scale` while the occupier window is active and the actor is its former owner.
3. × the base's `mind_control_scale` (riot, golden age).
4. × `atrocity_victim_scale` when the actor has victimized the target faction.

Each step truncates (`static_cast<int>(cost * scale)`), which equals the binary's integer ×2 and
/2; riot and golden age together multiply before truncating. The floor of 1 stays in
`ApplyProbeCostMultiplier_`. Unit quotes no longer apply Pact or atrocity.

`DiplomacyConfigParser` allowed only `commerce_rate` in status effects; it now allows the stats a
rule resolves toward a partner, `commerce_rate` and `mind_control_partner_scale`.

Wants-revenge ×1.5 needs a revenge record we do not keep; it goes under TODO in
[config/deviations/README.md](config/deviations/README.md) as a missing vanilla modifier, with a
TODO at `ApplyBaseRelationMods_`.

## Action numbers: probe_actions.json

Required fields on the action that uses them, rejected elsewhere (as `assimilation_turns_*` is):

| Action | Field | Shipping | Replaces |
| --- | --- | --- | --- |
| `sabotage_facility` | `protected_risk` | 2 | `MissionRisk`'s floor for an HQ or a `high_risk_sabotage` facility |
| `drain_energy` | `floor_morale_divisor` | 6 | `QuoteStealEnergy`'s `share * morale / 6` |
| `drain_energy` | `remainder_divisor` | 2 | `StealEnergyAmount`'s `remainderDraw / 2` |
| `genetic_plague` | `repeat_divisor` | 2 | the repeat-plague halving of kills and garrison damage |
| `genetic_plague` | `garrison_damage_divisor` | 2 | the garrison's `hp / 2` floor and draw bound |

Divisors, not scales: the binary divides integers, and these keep its truncation exact. The
rules take the action's config, as `MissionRisk` already does: `GeneticPlagueKilled`,
`QuoteStealEnergy`, and the new plague garrison rules. `StealEnergyQuote_t` carries the
remainder divisor, so `StealEnergyAmount` still needs no game.

## Calculations out of the mutations

`ProbeActionMutations.cpp` keeps only draws within limits a rule sets.

- **Plague garrison damage** (`DamagePlagueGarrison_`): new rules in `ProbeRules`, after the
  `QuoteStealEnergy` / `StealEnergyAmount` pattern:
  - `int PlagueGarrisonDrawBound(int hp)`: `hp / garrison_damage_divisor`, at least 1.
  - `int PlagueGarrisonDamage(const BaseManager& rBase, int hp, int draw)`: the floor plus the
    draw, divided by `repeat_divisor` on a repeat plague, times `plague_damage_scale`.
  The mutation draws in `[0, bound)` and applies the damage.
- **Mission and escape draws** (`MissionDrawSucceeded_`, `EscapeDrawSucceeded_`): the
  comparisons move to `ProbeRules` as `MissionDrawSucceeds(rChances, draw)` (`draw >= risk`) and
  `EscapeDrawSucceeds(rChances, draw)` (`draw > risk`). The mutation draws in
  `[0, probeStrength)` and `[0, escapeStrength)`; `ComputeProbeChances` already floors both at
  1, so the mutation's `std::max(1, ...)` goes.
- `ApplySaveAndBust_` already only rolls the chances a rule set; it stays.

## Tests

- Base quote: Pact's factor is the diplomacy fixture's `FactionPair` amount; riot halves and
  golden age doubles through the mood effects, and together leave it unchanged; former owner then
  golden age truncates in order; a unit quote ignores Pact, victim and its home base's riot.
- Parser: each new cost and action field is required where it applies and rejected elsewhere.
- Risk floor, drain amounts and plague kills/damage read their fixture numbers.
- Draw rules: `MissionDrawSucceeds` at `draw == risk` succeeds, `EscapeDrawSucceeds` at
  `draw == risk` fails.

## Docs

- [terranx-map.md](docs/thinker/terranx-map.md): the unit path reads no diplomatic status
  (`0x5A14BF`–`0x5A1800`).
- [effects-system.md](docs/architecture/effects-system.md): `MindControlScale` and
  `MindControlPartnerScale` in the probe-costs bullet.
- [diplomacy-system.md](docs/architecture/diplomacy-system.md): Pact's mind-control effect.
- [mechanic-decisions-review.md](docs/mechanic-decisions-review.md): PRB-1's risk floor and
  PRB-2's drain constants are now config.
- [config/deviations/README.md](config/deviations/README.md): TODO for wants-revenge ×1.5.

`./bd build` then `./bd test`.
