---
name: Probe formulas in Lua
overview: Move every probe formula out of ProbeRules into Lua expressions in probe_actions.json, evaluated by a ProbeCalculator, so the numbers inside them stop being separate config fields. Three steps - the two cost formulas, then drain and plague, then the odds.
todos:
  - id: calculator
    content: ProbeCalculator (GameDataContext-owned, const, plain-value inputs) over LuaRuntime, after CommerceCalculator
    status: completed
  - id: cost-formulas
    content: mind_control_base / total_thought_control / subvert_unit cost.formula; ProbeCostConfig_t loses energy_bias, dist_bias, mind_control_divisor, garrison_multiplier_base
    status: completed
  - id: drain-plague-formulas
    content: drain_energy share/floor/amount formulas; genetic_plague kill and garrison damage formulas; the four divisor fields go
    status: completed
  - id: odds-formulas
    content: success_formula becomes defense, mission_strength and escape_strength formulas; shown rates follow from the draw rules
    status: completed
  - id: tests-docs
    content: Calculator tests on fixture formulas; rule tests keep their expectations; effects/probe docs; ./bd build and ./bd test after each step
    status: completed
isProject: false
---

# Probe formulas in Lua

## Pattern

As `commerce.json`'s `formula` and `CommerceCalculator`: a formula is a Lua expression string
in the config that owns it, evaluated by `LuaRuntime::EvalInt` with snake_case variables. The
runtime provides `floor`, `ceil`, `max`, `min`, `abs`, `sqrt`. The binary divides integers, so
formulas write `floor(a / b)`; every input here is non-negative where it is divided, so `floor`
matches the binary's truncation.

`ProbeCalculator` (`include/game/probes/ProbeCalculator.h`), owned by `GameDataContext` beside
`commerceCalculator` and built from `LuaRuntime`. Its methods are `const`, take the config that
holds the formula plus a plain-value inputs struct, and return an `int`. It never reads the game
and never draws. `ProbeRules` gathers the inputs from the game, calls it through
`CurrentGameState().GetGameData().probeCalculator`, and keeps everything that is not arithmetic:
refusals, effect multipliers, clamps and draw outcomes.

Each formula is required by the parser on the action that uses it and rejected on any other, as
the per-action numbers are today. A formula that fails to evaluate throws (`EvalInt`), a config
error.

## Step 1: cost formulas

`cost` becomes `{"formula": "..."}` on the paid actions.

| Action | Shipping formula | Variables |
| --- | --- | --- |
| `mind_control_base`, `total_thought_control` | `(garrison_weight * (garrison_multiplier + 1) + floor(mind_control_total / 4) + population) * floor((energy + 1200) / (distance + 4))` | `garrison_weight`, `garrison_multiplier`, `mind_control_total`, `population`, `energy`, `distance` |
| `subvert_unit` | `mineral_cost * floor((energy + 800) / (distance + 2))` | `mineral_cost`, `energy`, `distance` |

- `garrison_weight` and `garrison_multiplier` are the sums of the two garrison stats over the
  tile; the `+ 1` that `garrison_multiplier_base` held is now in the formula.
- `distance` arrives already scaled by `probe_distance_scale` and truncated. The HQ-tile refusal
  (raw distance 0) stays in `QuoteBaseActionCost_`.
- The result goes through `ApplyProbeCostMultiplier_` with the target and actor cost stats, as
  today.

`ProbeCostConfig_t` holds `formula` only. `energy_bias`, `dist_bias`, `mind_control_divisor` and
`garrison_multiplier_base` go, with their parser checks. `MindControlFormula_` and
`SubvertFormula_` go. `SumMindControlGarrison_` adds the two sums into the formula inputs instead of returning their product.

Calculator methods: `int MindControlCost(const ProbeCostConfig_t&, const MindControlCostInputs_t&)`
and `int SubvertCost(const ProbeCostConfig_t&, const SubvertCostInputs_t&)`.

## Step 2: drain and plague formulas

**Drain energy** (`QuoteStealEnergy`, `StealEnergyAmount`):

| Field | Shipping formula | Variables |
| --- | --- | --- |
| `share_formula` | `floor(treasury * population / (faction_population + 1))` | `treasury`, `population`, `faction_population` |
| `floor_formula` | `floor(share * max(1, morale) / 6)` | `share`, `morale` |
| `amount_formula` | `floor_amount + floor(draw / 2)` | `floor_amount`, `draw` |

The draw bound stays `share - floor_amount` (the binary's `game_randv(share − floor)`), at least
0. `StealEnergyAmount` keeps clamping to the treasury and storing 0 for a result of 1: those are
the binary's `range` and its store quirk, not arithmetic a modder tunes. `StealEnergyQuote_t`
carries the share and floor; `remainderDivisor` goes. `StealEnergyAmount` takes the action
config so it can evaluate `amount_formula`.

**Genetic plague** (`GeneticPlagueKilled`, `PlagueGarrisonDrawBound`, `PlagueGarrisonDamage`):

| Field | Shipping formula | Variables |
| --- | --- | --- |
| `kill_formula` | `floor((floor(population * (max(1, defense_techs) - min(max(known_defense_techs, 1), max(1, defense_techs))) / max(1, defense_techs)) + 1) / (repeated + 1))` | `population`, `defense_techs`, `known_defense_techs`, `repeated` (0 or 1) |
| `garrison_damage_bound_formula` | `max(1, floor(hp / 2))` | `hp` |
| `garrison_damage_formula` | `floor((floor(hp / 2) + draw) / (repeated + 1))` | `hp`, `draw`, `repeated` |

`repeated` is 1 once the base has had a plague; Lua reserves `repeat`. The action holds them as
`std::optional<DrainFormulas_t> drain` and `std::optional<PlagueFormulas_t> plague`. The rules keep the
`plague_damage_scale` multiplier and the kill cap of size − 1. `repeat_divisor`,
`garrison_damage_divisor`, `floor_morale_divisor` and `remainder_divisor` go.

## Step 3: odds formulas

`success_formula` becomes three formulas:

| Field | Shipping formula | Variables |
| --- | --- | --- |
| `defense` | `min(0, max(-2, probe_defense))` | `probe_defense` (the target's `probe_defense` stat: SE Probe plus Covert Ops) |
| `mission_strength` | `max(1, floor(max(1, morale) / 2) - defense + 1)` | `morale`, `defense` |
| `escape_strength` | `max(1, max(1, morale) - defense)` | `morale`, `defense` |

`morale` is the probe's effective morale level as the rules resolve it; the formulas floor it at
1, as `ComputeProbeChances` did. A strength formula that returns less than 1 throws: the
strengths are exclusive draw bounds and the rates' divisors.

The shown rates are the draws' own probabilities, so they follow from the strengths and the draw
rules instead of separate formulas. The mission fails when its draw in `[0, mission_strength)` is
below the risk: failure `floor(risk * 100 / mission_strength)`. The escape is lost when its draw
is at or below the risk: loss `floor((risk + 1) * 100 / escape_strength)`. Those two expressions
stay in `ComputeProbeChances`, beside `MissionDrawSucceeds` and `EscapeDrawSucceeds`, which they
describe. Negative risk still shows 100%.

`ProbeSuccessFormula_t`'s four numbers (`morale_divisor`, `strength_offset`, `defense_clamp_min`,
`defense_clamp_max`) go. `ComputeProbeChances` takes the two strengths instead of morale,
defense and the formula struct; `ProbeChancesFor` gets them from the calculator.
`TargetProbeDefenseEffect` returns the evaluated `defense`. `ProbeChance_t::targetProbeEffect`,
which nothing read, goes. A negative risk now draws from the mission strength rather than the
raw morale; no shipping action has one.

## Config

- [probe_actions.json](config/probe_actions.json) and its fixture: the formulas above replace
  the cost numbers, the drain and plague divisors, and `success_formula`'s numbers.

## Tests

- `ProbeCalculator`: each method evaluates its fixture formula on chosen inputs; a formula that
  references an unknown variable throws.
- Parser: each formula is required on its action and rejected elsewhere; `success_formula`
  requires all three.
- Existing quote, drain, plague and odds tests keep their expectations: the fixture formulas
  reproduce today's arithmetic. Tests that call `ComputeProbeChances` with a morale pass
  strengths instead.
- A test with an edited fixture formula shows a quote, a drain and a strength follow the config.

## Docs

- [effects-system.md](docs/architecture/effects-system.md): the probe-costs bullet names the
  cost formulas as the formula inputs' readers.
- [player-interaction-system.md](docs/architecture/player-interaction-system.md) or the probe
  section it links, wherever the odds formula is described.
- [high-level.md](docs/architecture/high-level.md): `ProbeCalculator` among the calculators.

`./bd build` then `./bd test` after each step.
