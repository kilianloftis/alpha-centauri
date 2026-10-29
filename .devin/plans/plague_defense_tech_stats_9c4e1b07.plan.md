---
name: Plague defense tech stats
overview: Genetic plague's defense count becomes the tech-only stat plague_defense. Research owns the tech-stat sums that plague and commerce both read. Plague names share the plague_ prefix, the unread gene-warfare flags go, and the action unlocks with Retroviral Engineering.
todos:
  - id: confirm-binary
    content: Confirm the 0x10 menu gate and the 0x20 defense count in terranx.exe; record both in terranx-map.md
    status: pending
  - id: research
    content: TechStatTotal and DiscoveredTechStatTotal in ResearchRules; the plain-Add filter moves there from CommerceRules
    status: pending
  - id: stat
    content: StatId_t::PlagueDefense (plague_defense); remove IncGeneWarfareDefense and AllowGeneWarfare
    status: pending
  - id: validator
    content: EffectReferenceValidator refuses plague_defense outside tech effects
    status: pending
  - id: plague
    content: Renames across config, ProbeCalculator and ProbeRules; PlagueKilled reads the research sums; required_tech retroviral_engineering
    status: pending
  - id: commerce
    content: TechDenominator_ sums DiscoveredTechStatTotal over factions
    status: pending
  - id: tests-docs
    content: Research, validator, plague and commerce tests; effects-system.md, economy-system.md, terranx-map.md; ./bd test
    status: pending
isProject: false
---

# Plague defense tech stats

Genetic plague's kill formula divides the defense techs the target knows by the defense techs in
the game. Both counts become sums of the tech-only stat `plague_defense`, computed by the research
subsystem, which also computes commerce's planet-wide tech total. Adding a defense tech is one
`techs.json` entry: it raises the target's defense and the total together. A non-tech source of
`plague_defense` fails at load.

## Confirmed in the binary

Confirm in `terranx.exe` (`sha1` `4b19c1fe3266b5ebc4305cd182ed6e864e3a1c4a`), then record under
"Probe: action ids, HSA, odds, and the roll" in [terranx-map.md](docs/thinker/terranx-map.md).
Thinker's `probe.cpp` gives the expected shape:

- `probe` (`0x59F120`) walks every tech. A tech the actor knows with `TFLAG_ALLOW_GENE_WARFARE`
  (`0x10`) offers menu item 7, Introduce Genetic Plague (`probe.cpp:423`, `:476`).
- In the same walk, every tech with `TFLAG_INC_GENE_WARFARE_DEFENSE` (`0x20`) counts toward the
  total, and the ones the target knows count toward the known number (`probe.cpp:426`–`:429`).
- `alpha.txt` and `alphax.txt` flag `0x20` on Biogenetics, Gene Splicing, Bio-Engineering,
  Biomachinery, Matter Editation and Retroviral Engineering. Only Retroviral Engineering carries
  `0x10`.

## Research sums

[ResearchRules](include/game/research/ResearchRules.h):

- `int TechStatTotal(const TechRegistry& rTechs, StatId_t stat)`: the sum of `stat`'s plain Add
  amounts over every tech in the registry.
- `int DiscoveredTechStatTotal(const ResearchManager& rResearch, StatId_t stat)`: the same sum
  over the faction's discovered techs, skipping effects whose `removed_by_tech` it has discovered.
- A plain Add is a `StatModifier` of `stat` with op `Add` and no `condition`, `buildingFilter`,
  `factionFilter`, `amount_source` or `selector`. That filter is `PlanetWideCommerceAdd_` today; it
  moves out of [CommerceRules.cpp](src/game/economy/CommerceRules.cpp) into `ResearchRules.cpp`.
  Each amount goes through `FinalizeResolvedStat`, as commerce does now.

## The stat

[EffectEnums.h](include/game/effects/EffectEnums.h):

- `StatId_t::PlagueDefense`, parsed from `"plague_defense"`. Additive, in the same domain group as
  `CommerceRating`.
- Remove `RuleFlagId_t::IncGeneWarfareDefense` and `RuleFlagId_t::AllowGeneWarfare` and their parse
  strings. Nothing reads either one.
- `IsTechOnlyStat(StatId_t)`: true for `PlagueDefense`.

[EffectReferenceValidator.cpp](src/game/EffectReferenceValidator.cpp): `EffectSourceChecker_`
gets the named sibling `TechEffects` for the tech loop in `CheckMapSources_`. `Effects`, which
every other source uses, throws on a `StatModifier` whose stat `IsTechOnlyStat`:
`"<source>: plague_defense counts toward genetic plague only from techs; use plague_damage_scale
on buildings, units or social engineering"`.

## Config

[config/techs.json](config/techs.json) and [tests/fixtures/techs.json](tests/fixtures/techs.json):
each `inc_gene_warfare_defense` rule flag becomes

```json
{"type": "StatModifier", "scope": "FactionGlobal",
 "parameters": {"stat": "plague_defense", "amount": 1, "op": "Add"}}
```

Shipping: the six techs above. Retroviral Engineering's `allow_gene_warfare` flag is deleted.

`genetic_plague` in [config/probe_actions.json](config/probe_actions.json) and
[tests/fixtures/probe_actions.json](tests/fixtures/probe_actions.json):

| Key or variable | New name |
|---|---|
| `kill_formula` | `plague_kill_formula` |
| `garrison_damage_formula` | `plague_garrison_damage_formula` |
| `garrison_damage_bound_formula` | `plague_garrison_damage_bound_formula` |
| `defense_techs` | `plague_defense_total` |
| `known_defense_techs` | `plague_defense` |

Shipping `required_tech` is `retroviral_engineering`.

## Plague code

- [ProbeActionConfigParser.cpp](src/game/probes/ProbeActionConfigParser.cpp) requires the renamed
  keys.
- [ProbeCalculator](include/game/probes/ProbeCalculator.h): `PlagueKillInputs_t` fields
  `defenseTotal` and `defense`, bound to `plague_defense_total` and `plague_defense`.
- [ProbeRules](include/game/probes/ProbeRules.h): `GeneticPlagueKilled` becomes `PlagueKilled`.
  `defenseTotal` is `TechStatTotal(registry, PlagueDefense)` and `defense` is
  `DiscoveredTechStatTotal(target research, PlagueDefense)`. The tech loop and its `if (pTechs)`
  check go. Callers in [ProbeActionMutations.cpp](src/game/probes/ProbeActionMutations.cpp)
  follow the rename.
- Header comments that name `kill_formula`, `garrison_damage_formula` or
  `garrison_damage_bound_formula` use the new keys.

## Commerce

`TechCommerceRatingContribution_` is deleted. `TechDenominator_` sums
`DiscoveredTechStatTotal(rFaction.GetResearch(), StatId_t::CommerceRating)` over factions, plus
the +1 baseline, clamped to at least 1. Commerce values do not change.

## Tests

- New `tests/game/ResearchRulesTests.cpp` (listed in [tests/CMakeLists.txt](tests/CMakeLists.txt)):
  both sums over a fixture registry. Conditional, filtered, `amount_source`, selector and non-Add
  modifiers do not count. `removed_by_tech` drops an effect from the discovered sum only.
- Validator: `plague_defense` on a building throws, and on a tech it loads.
- [ParserTests.cpp](tests/effects/ParserTests.cpp): `plague_defense` parses; the two flag checks go.
- [ProbeRulesTests.cpp](tests/game/ProbeRulesTests.cpp),
  [ProbeCalculatorTests.cpp](tests/game/ProbeCalculatorTests.cpp),
  [ProbeActionTests.cpp](tests/game/ProbeActionTests.cpp): the renamed keys, variables and
  function. A plague kill test whose target knows one of the fixture's two defense techs still
  kills the same count.
- Commerce tests pass unchanged.

## Docs

- [effects-system.md](docs/architecture/effects-system.md): the rule-flag paragraph names
  `plague_defense` as a tech-only stat and drops `allow_gene_warfare`; the genetic plague line
  names `ProbeRules::PlagueKilled` and the research sums.
- [economy-system.md](docs/architecture/economy-system.md): **totalCommerceTech** is
  `DiscoveredTechStatTotal` per faction.
- [terranx-map.md](docs/thinker/terranx-map.md): the two flag sites from "Confirmed in the binary".

`./bd test`, then `./bd build`.
