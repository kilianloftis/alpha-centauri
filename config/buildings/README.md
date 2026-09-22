# Building Configuration

All `.json` files in this directory are loaded alphabetically and merged into the building registry at startup. Modders can add new files here without modifying any existing file.

---

## File Structure

Each file must be a JSON array of building objects. Any number of files may coexist — `buildings.json` for base buildings, `projects.json` for Secret Projects, and any additional mod files.

---

## Building Object Fields

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `id` | string | Yes | — | Unique identifier used in code and save files |
| `name` | string | No | `id` | Display name shown in the UI |
| `mineral_cost` | int | No | `0` | Minerals required to construct; must be a non-negative integer |
| `upkeep` | int | No | `0` | Base energy credits charged per turn per owned **constructed** copy. Effective cost is RawScaled `FacilityEnergyUpkeep` (seed = `upkeep`; techs use `buildingFilter` to target All / BuildingId / Category). Continuous `GrantBuilding` expansions are not constructed and pay nothing. UI: `BuildingConfig_t::GetUpkeep()` (base), `BaseManager::GetBuildingUpkeepByType()` / `Faction::GetBuildingUpkeepByType()` (resolved). |
| `required_tech` | string | No | `""` | Tech that must be discovered before the building is available; omit or empty = always available |
| `allow_multiple` | bool | No | `false` | If true, a base may build more than one copy |
| `secret_project` | bool | No | `false` | If true, only one faction in the world may own this building |
| `orbital` | bool | No | `false` | If true, ownership counts are public to all factions (satellite census) |
| `effects` | Effect[] | No | `[]` | Structured list of gameplay effects (see below) |

A flat per-turn bonus (the old `nutrients_bonus`) is a `StatModifier` effect with `scope: "ThisBase"`. A per-improvement bonus (the old `improvement_bonuses`) is a `TileYieldModifier` effect with a `HasImprovement` selector — see Effect Types below.

### Load-time validation

The parser fails the load rather than substituting a default, so a setting either applies or the game does not start:

- **Unknown keys are rejected.** A typo'd `allow_multiples` is an error naming the building and the key, not a silently ignored line.
- **Wrong-typed values are rejected.** `"allow_multiple": "yes"` is an error, where it used to parse as `false`.
- **`mineral_cost` must be a non-negative integer.**
- **`upkeep` must be a non-negative integer.**
- **`secret_project` and `allow_multiple` are mutually exclusive.** A secret project is unique in the world, so "more than one copy" is unexpressible.

### Uniqueness at runtime

A building that is not `allow_multiple` cannot be added twice to one base, and a `secret_project` cannot be added anywhere once any faction owns it *or* once a built copy has been destroyed (destruction tombstones it — nobody rebuilds it). This is enforced where a building is granted, so it also covers projects completed by production and buildings granted by another building's effect.

Callers that can lose the race check `BuildingManager::CanAddBuilding` first: production drops the item and reports it rather than failing the turn. `AddBuilding` still throws if the invariant is violated, as a programmer-error backstop.

---

## Effects

Each entry in `effects` describes a single gameplay effect applied when the building is present. All `effects` arrays — for buildings, unit components, and any future effect source — are parsed by the single shared `EffectConfigParser` (`include/game/effects/EffectConfigParser.h`), so the schema below applies everywhere, not just to buildings.

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `type` | string | Yes | — | Effect category (see Types below) |
| `scope` | string | Yes | — | Who is affected (see Scopes below) |
| `condition` | object | No | absent | Optional gate (`kind` SubjectDomain / HasComponent / HasFlag / IsPrototype / IsCombatUnit / AttackerDomain / AllOf / …); effect is suppressed when unsatisfied |
| `buildingFilter` | object | No | absent (= all buildings) | Restricts which building types receive FacilityEnergyUpkeep (and similar) modifiers: `{ "kind": "All" }`, `{ "kind": "BuildingId", "building": "..." }`, or `{ "kind": "Category", "category": "grow" }` |
| `parameters` | object | No | `{}` | Key/value strings interpreted by the effect handler |

### Effect Types

| Value | Description |
|---|---|
| `GrantBuilding` | Expands another building's effects onto this one by ID (`parameters.building_id`) — no constructed copy, no maintenance. To actually build it, use the triggered `AddBuilding` instead. |
| `StatModifier` | Adds or multiplies a named stat (`parameters.stat`, `parameters.amount`, `parameters.op`) |
| `RuleFlag` | Enables a named gameplay rule (`parameters.flag`) |
| `SocialEngineeringOverride` | Forces a social engineering value (`parameters.category`, `parameters.value`) |
| `DiplomaticModifier` | Adjusts diplomatic standing (`parameters.target_faction_id`, `parameters.value`) |
| `TileYieldModifier` | Modifies the yield of selected tiles (`parameters.resource`, `parameters.selector`, `parameters.amount`, `parameters.op`) — see below |
| `OrbitalAttack` | ASAT charge against other factions' `orbital` buildings (`parameters.chance`, `parameters.cooldown_turns`, `parameters.chance_of_destruction_on_fail`) |
| `Intercept` | Pre-combat intercept (`parameters.chance`, optional `cooldown_turns`, `chance_of_destruction_on_fail`; requires `condition`) |
| `Scramble` | Unit may scramble to become the combat defender (`parameters.range`; requires `condition` on the attacker) |

`amount`/`value` accept either a JSON number or a numeric string. `op` is one of `Add`, `AddPercent` (amount in percent points, e.g. `25` = +25%), `MultiplyGeometric` (factor form, e.g. `0.5`), `MaxClamp`, `MinClamp` — defaults to `Add`.

Any effect may carry an optional top-level `condition` object making it situational, e.g. a combat bonus that only applies against certain targets:

```json
{
  "type": "StatModifier",
  "scope": "ThisUnit",
  "parameters": { "stat": "attack", "amount": 25, "op": "AddPercent" },
  "condition": { "kind": "TargetTileHas", "value": "Base" }
}
```

`TargetTileHas` matches `value` against the target tile's features via `Tile::HasFeature` — terrain (`Rocky`), `River`/`Fungus`, landmark, bonus, or any improvement id including `Base`. Conditional effects are excluded from context-free resolution and only apply through a matching runtime context (e.g. `Unit::GetAttackAgainst`).

#### `TileYieldModifier` selector

```json
"parameters": {
  "resource": "nutrients",
  "selector": { "kind": "HasImprovement", "improvement": "Farm" },
  "amount": 1,
  "op": "Add"
}
```

`selector.kind` is `BaseTile` (the base's own tile; no `improvement` needed) or `HasImprovement` (any worked tile with the given `selector.improvement`, e.g. `Farm`/`Condenser`).

### Scopes

| Value | Description |
|---|---|
| `ThisBase` | Only the base that built this building |
| `AllOwnerBases` | Every base the owning faction controls |
| `ThisUnit` | Only the unit the component belongs to (unit component effects only) |
| `FactionUnits` | Every unit the owning faction controls |
| `FactionGlobal` | A faction-wide capability, not tied to a specific base or unit |
| `WorldGlobal` | Affects every faction, not just the owner |
| `ThisPop` | Only the specific pop instance the effect belongs to (pop type tile-multiplier effects only) |

### `on_complete_effects`

Entries in `effects` are **continuous**: active for as long as the building stands. A building
may also declare `on_complete_effects`, a separate array of **one-shot** effects that fire once,
when the facility is completed here. Which list an effect sits in is what says when it fires —
there is no `persistence` field.

One-shot entries carry no `scope`, `condition`, `radius` or filters: they act on the base that
just completed the building. They accept an optional `once_per`
(`{ "scope": "unit" | "base" | "faction", "key": "..." }`, both fields required) to fire at
most once per subject — and only an entry that actually changed something spends its key.

| Type | Description |
|---|---|
| `AddBuilding` | Constructs another building here (`parameters.building_id`); the real facility pays upkeep |
| `GrantTech` | Grants a technology to the owning faction (`parameters.tech_id`) |
| `GrantUnit` | Spawns units assembled from `parameters.component_ids` (optional `parameters.count`, default 1), homed at this base but not built there — no train bonuses, no starting experience |
| `GrantEnergy` | Credits the faction treasury (`parameters.amount`) |
| `ModifyPopulation` | Changes base size (`parameters.amount`, `op`, `min_size`) — the colony pod's cost |
| `DestroyFacility` | Destroys random facilities (`parameters.count`, `exclude_hq`, `exclude_secret_projects`) |
| `SetInfiltration` | Writes lasting datalink infiltration; optional `factionFilter` picks the targets (absent = every other faction). This is the only type that accepts a `factionFilter` |
| `Rebel` | Hands the base to a weighted other faction |

---

## Examples

### Regular building

```json
{
  "id": "Recycling_Tanks",
  "name": "Recycling Tanks",
  "mineral_cost": 5,
  "upkeep": 2,
  "required_tech": "ecology",
  "effects": [
    {
      "type": "StatModifier",
      "scope": "ThisBase",
      "parameters": { "stat": "nutrients", "amount": "1" }
    }
  ]
}
```

### Secret Project

```json
{
  "id": "Human_Genome_Project",
  "name": "Human Genome Project",
  "mineral_cost": 200,
  "upkeep": 2,
  "secret_project": true,
  "required_tech": "biogenetics",
  "effects": [
    {
      "type": "RuleFlag",
      "scope": "FactionGlobal",
      "parameters": { "flag": "population_boom" }
    }
  ]
}
```

### Stockpile items

Never-completing production items live in `config/stockpiles.json`, not here — they are not
buildings and share none of a building's fields. See `config/README-stockpiles.md`.

### One-shot grant on completion

```json
{
  "id": "Merchant_Exchange",
  "name": "Merchant Exchange",
  "mineral_cost": 80,
  "upkeep": 2,
  "secret_project": true,
  "required_tech": "industrial_economics",
  "on_complete_effects": [
    { "type": "AddBuilding", "parameters": { "building_id": "Energy_Bank" } }
  ]
}
```
