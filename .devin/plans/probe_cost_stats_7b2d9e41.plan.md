---
name: Probe cost stats
overview: Collapse the probe cost multipliers into one target-side stat and one actor-side stat, and the two distance scales into one, so scope decides which quote a factor reaches, every factor is an effect in config, and the per-rule stats and cost numbers go.
todos:
  - id: conditions
    content: PartnerPlayerType, PartnerIsFormerOwner, VictimizedPartner and TargetIsBase effect conditions (parser, evaluator, validator)
    status: completed
  - id: target-stat
    content: probe_action_cost read on the target base or the target unit; absorbs mind_control_scale, subversion_cost_scale and former_owner_scale; SE Probe levels become MultiplyGeometric with FactionUnits copies
    status: completed
  - id: actor-stat
    content: probe_actor_cost on the actor toward the target takes Pact, the AI-vs-player discount and atrocity victim; mind_control_partner_scale, ai_probe_cost_scale and atrocity_victim_scale go
    status: completed
  - id: distance-stat
    content: probe_distance_scale read on the target base or unit replaces mind_control_distance_scale and subversion_distance_scale; Punishment Sphere reaches its homed units through FactionUnits OriginBaseIsHomeBase
    status: completed
  - id: tests-docs
    content: Requirement tests per factor and per side; effects-system, diplomacy, difficulty docs; ./bd build and ./bd test
    status: completed
isProject: false
---

# Probe cost stats

## Target shape

Three stats, each read on one side of the quote. Scope routes each factor to the quotes it
belongs to: a base's effect list holds that base's `ThisBase` effects and the faction-wide
lane (`FactionGlobal`, `AllOwnerBases`, `WorldGlobal`); a unit's list holds `ThisUnit` and
`FactionUnits` (`FilterForBase`, `CollectLiveUnitEffects`).

| Stat | Domain | Base quote reads it on | Unit quote reads it on |
| --- | --- | --- | --- |
| `probe_action_cost` | Base | the target base, `ctx.pPartner` the actor | the target unit, `ctx.pPartner` the actor |
| `probe_distance_scale` | Base | the target base | the target unit |
| `probe_actor_cost` | Faction | the actor, `ctx.pPartner` the target faction, `ctx.pBase` the target base | the actor, `ctx.pPartner` the target faction |

`DomainFor` stays Base for the two target-side stats; it names the amount-source subject and
guards the typed `Resolve*Stat` entry points, not every reader. The unit quote filters the unit's
live effects with `FilterByStatIdInContext` and resolves with `ResolveStatModifiers`, as
`AwayFromHomeDrones` resolves a unit stat from a base's list. A `WorldGlobal` modifier on these
stats reaches only base quotes, since unit lists take `WorldGlobal` stat modifiers of their own
domain only.

Distance stays its own stat: it sits in the divisor `(dist + dist_bias)`, so a distance factor is
not a cost factor.

Combining a stat's factors before truncating replaces the binary's integer step per factor. A
quote with several halvings and doublings can differ from the binary by 1.

## Factors and where they move

| Factor (binary) | Today | After |
| --- | --- | --- |
| SE Probe table, both quotes | `probe_action_cost` `AddPercent`, read at the target base or a unit's home base | `probe_action_cost` `MultiplyGeometric` 0.5 / 0.75 / 1.5 / 2: `FactionGlobal` (bases) and a `FactionUnits` copy (units) per level |
| Drone riot ÷2, golden age ×2, base only | `mind_control_scale` mood effects | `probe_action_cost`, same `ThisBase` mood effects |
| Former owner ÷2, base only | `former_owner_scale` cost number | `probe_actions.json` effect: `AllOwnerBases` `probe_action_cost` ×0.5, condition `PartnerIsFormerOwner` |
| Plan baseline ÷2, Poly ×2, unit only | `subversion_cost_scale` | `probe_action_cost`, same `FactionUnits` baseline and `ThisUnit` Poly |
| Nethack ×3/4, both quotes | `probe_actor_cost` | unchanged |
| Pact ×2, base only | `mind_control_partner_scale` `FactionPair` | `diplomacy.json` Pact `FactionPair` `probe_actor_cost` ×2, condition `TargetIsBase` |
| AI vs player ×3/diff, both quotes | `ai_probe_cost_scale` on player factions, read for AI actors in C++ | `difficulty.json` thinker/transcend: `FactionGlobal` `probe_actor_cost` ×0.75 / ×0.6, `factionFilter` `PlayerType` AI, condition `PartnerPlayerType` Player |
| Atrocity victim ×2, base only | `atrocity_victim_scale` cost number | `probe_actions.json` effect: `FactionGlobal` `probe_actor_cost` ×2, condition `AllOf` [`TargetIsBase`, `VictimizedPartner`] |
| Genejack ×2, Crèche ÷2, base distance | `mind_control_distance_scale` `ThisBase` | `probe_distance_scale` `ThisBase` |
| Punishment Sphere ÷2, both distances | `mind_control_distance_scale` `ThisBase` and `subversion_distance_scale` read at the unit's home base | `probe_distance_scale` `ThisBase` (base) and `FactionUnits` with condition `OriginBaseIsHomeBase` (units homed there) |

Removed: `mind_control_scale`, `subversion_cost_scale`, `mind_control_partner_scale`,
`ai_probe_cost_scale`, `mind_control_distance_scale`, `subversion_distance_scale`,
`former_owner_scale`, `atrocity_victim_scale`, `AiActorCostScale_`, `ApplyBaseRelationMods_`,
`ResolveSubversionDistanceScale_`. `garrison_probe_weight` and `garrison_probe_multiplier` stay:
they are formula inputs, not multipliers.

## Conditions

New `Condition_t` alternatives, each failing closed without its subject:

- `PartnerPlayerType` (`{"kind": "PartnerPlayerType", "type": "Player"}`): `ctx.pPartner`'s control
  matches, as `factionFilter` `PlayerType` does for the owner.
- `PartnerIsFormerOwner`: `ctx.pBase`'s occupier window is active and its former owner is
  `ctx.pPartner`.
- `VictimizedPartner`: `ctx.pFaction` has victimized `ctx.pPartner` (`AtrocityLedger::HasVictimized`).
- `TargetIsBase`: `ctx.pBase` is set. On the actor-side resolve that means a base target.

All four are situational, so `ConditionNeedsSituationalContext` returns true and
`CollectLiveUnitEffects` keeps them for the in-context resolve. Parser, `ActiveEffect.cpp`
evaluator and `EffectReferenceValidator` gain an arm each.

## Rules

`QuoteProbeActionCost` multiplies the target-side and actor-side resolves and applies the
result once with `ApplyProbeCostMultiplier_`:

- Base quote: `probe_action_cost` and `probe_distance_scale` on the base (`ResolveBaseStat`,
  `ctx.pBase` the base, `ctx.pPartner` the actor).
- Unit quote: the same two stats on the unit, by hand as above (`ctx.pUnit` the unit,
  `ctx.pPartner` the actor). The home base is no longer read.
- `probe_actor_cost`: `ResolveFactionStat(actor effects, ...)` with `ctx.pFaction` the actor,
  `ctx.pPartner` the target faction, and `ctx.pBase` the target base for a base quote.

`ProbeCostConfig_t` keeps the formula fields only (`energy_bias`, `dist_bias`,
`mind_control_divisor`, `garrison_multiplier_base`).

## Config

- [social_rating_effects.json](config/social_rating_effects.json) Probe levels: each level's
  `probe_action_cost` becomes `MultiplyGeometric` (−2 0.5, −1 0.75, +1 1.5, +2 2) and gains a
  `FactionUnits` copy. Same in both fixtures.
- [pop_composition.json](config/pop_composition.json) riot tiers and golden age:
  `mind_control_scale` becomes `probe_action_cost`. Same in the fixtures.
- [probe_actions.json](config/probe_actions.json) `effects`: the plan baseline becomes
  `probe_action_cost`; the former-owner and victim entries above. Costs drop
  `former_owner_scale` and `atrocity_victim_scale`.
- [abilities.json](config/unit_components/abilities.json) Poly: `subversion_cost_scale` becomes
  `probe_action_cost`.
- [buildings.json](config/buildings/buildings.json): Genejack, Crèche and Punishment Sphere
  emit `probe_distance_scale`; Punishment Sphere adds the `FactionUnits` `OriginBaseIsHomeBase`
  entry. Same in the fixtures.
- [diplomacy.json](config/diplomacy.json) Pact: `mind_control_partner_scale` becomes
  `probe_actor_cost` with condition `TargetIsBase`. `DiplomacyConfigParser`'s partner-stat list
  becomes `commerce_rate` and `probe_actor_cost`, and it accepts the `TargetIsBase` condition on
  that stat only.
- [difficulty.json](config/difficulty.json) thinker / transcend: the AI-vs-player entries move to
  AI factions as above.

## Tests

- Each factor applies to the quote it belongs to and not the other: riot, golden age, former
  owner, Genejack and Crèche reach a base and not a unit homed there; Pact and victim reach a base
  and not a unit; plan baseline and Poly reach a unit and not a base; SE Probe, Nethack, the AI
  discount and Punishment Sphere reach both.
- Side: an actor's own SE Probe or riot never changes what it pays; a target's Nethack never
  changes what it is charged.
- Conditions: each new kind parses, fails closed without its subject, and matches.
- Existing quote tests keep their expectations except where a combined factor now truncates once.

## Docs

- [effects-system.md](docs/architecture/effects-system.md): the three stats and four conditions
  replace the probe-costs bullet's per-rule stats; the probe quotes join `AwayFromHomeDrones` as
  readers outside their stat's `DomainFor`.
- [diplomacy-system.md](docs/architecture/diplomacy-system.md): Pact's `probe_actor_cost`.
- [difficulty-system.md](docs/architecture/difficulty-system.md): the AI-vs-player discount.

`./bd build` then `./bd test`.

## SE expansion into base lists

`ResolveSocialRatingLevelEffects` appended every effect of a rating level to the base's list,
`FactionUnits` ones included. With SE Probe's `FactionUnits` copy that counted the level twice on
a base quote. It now leaves out `FactionUnits` effects, as `FilterForBase` does; units get them
through `ExpandFactionLaneSocialRatingEffects`.

## Difficulty

The easy-difficulty AI-bases −50% (`difficulty.json` citizen, specialist, talent) goes: neither
binary cost path has it. The only difficulty reads are the AI-vs-player ×3/diff (`0x59ED2B`,
`0x5A158F`) and the AI's own choice to attempt mind control (`0x5A2A78`). Same in the difficulty
fixtures.
