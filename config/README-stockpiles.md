# Stockpile items (`config/stockpiles.json`)

A stockpile is a **never-completing production item**: a base can queue it like a building or
a unit, but it never finishes. Each turn it converts whatever minerals are left after unit
support into other resources, and stays queued until the player picks something else.

A stockpile is deliberately *not* a building. It is never constructed, so it has no mineral
cost, no upkeep, no `allow_multiple`, no `secret_project`, and no `orbital` — those keys do
not exist here and are rejected as unknown.

## Fields

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `id` | string | Yes | — | Unique identifier used in code and save files |
| `name` | string | No | `id` | Display name shown in the build menu |
| `required_tech` | string | No | `""` | Tech that must be discovered before this stockpile is selectable; omit or empty = always available |
| `fallback_priority` | int | No | `0` | Ranking for the empty-queue default: among available stockpiles the highest wins, ties broken by load order |
| `rounding` | string | **Yes** | — | How a fractional yield becomes an integer: `"down"`, `"up"`, or `"nearest"` |
| `effects` | Effect[] | Yes | — | Must contain at least one `StatModifier` with `amount_source: "MineralsConverted"` |

## The conversion

Yield per stat is `mineralsConverted × amount`, rounded once per stat by `rounding`.

- **Output stats** are `nutrients`, `energy`, `econ`, `labs`, and `psych`. `minerals` is
  rejected: it is the input, so converting to it is a loop.
- **`energy` behaves like collected tile energy**, because at a base it is not a bank: the
  converted amount has inefficiency applied (distance to HQ, waived at the HQ itself) and is
  then split into econ / labs / psych by the player's slider allocation. Nothing is lost to
  rounding — econ is the residual bucket, so the three shares always sum to the post-
  inefficiency amount.
- **`econ` skips the sliders and inefficiency** and reaches the treasury whole via
  `IncomeCollection` the same turn. This is what stock `Stockpile_Energy` uses: the player
  gets every credit regardless of where the sliders sit or how far the base is from the HQ.
  Use `energy` when you want a stockpile to feed research and psych alongside the treasury.
- **Multiple `MineralsConverted` modifiers** on different stats credit multiple outputs from
  the same minerals.
- **`AddPercent` / `MultiplyGeometric` modifiers** on an output stat scale the converted
  amount. A plain `Add` **without** `amount_source` is rejected: it would pay out on turns
  with nothing to convert, which is a stipend rather than a conversion.
- Yield is resolved against **this config's own effects only**, never the base effect pool.
  Those base modifiers already apply to the banks being credited, so folding them in would
  count them twice. To boost conversion, put the modifier on the stockpile item.

`rounding` has no default on purpose. A rate of `0.5` does not say what 5 minerals are worth
until the rounding is stated, and that is balance — so the parser fails the load rather than
assuming. Stock `Stockpile_Energy` is `"up"` at 0.5 econ per mineral, so an odd mineral count
rounds in the player's favour (5 minerals → 3 econ).

## Turn order

Conversion runs in the `SurplusConversion` turn stage, which `config/turn_stages.json` orders
after `UnitSupport` (so support claims its minerals first) and before `IncomeCollection` /
`ResearchAccumulation` (so converted econ and labs are spent the same turn). Moving that
stage later silently delays every stockpile's output by a turn.

## The empty-queue default

When a base has nothing queued — a new base, a completed item, a probe wiping production — it
falls back to the available stockpile with the highest `fallback_priority`. Falling back never
charges the retool penalty, since the player did not choose it. If no stockpile is available
(all tech-gated, or none configured), the queue stays empty and surplus minerals are wasted.

## Example

```json
[
  {
    "id": "Stockpile_Energy",
    "name": "Stockpile Energy",
    "rounding": "down",
    "effects": [
      {
        "type": "StatModifier",
        "scope": "ThisBase",
        "parameters": {
          "stat": "econ",
          "amount": 0.5,
          "amount_source": "MineralsConverted",
          "op": "Add"
        }
      }
    ]
  }
]
```
