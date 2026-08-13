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
| `mineral_cost` | int | No | `0` | Minerals required to construct; must be a non-negative integer. **Omit on stockpile items** — the field does not apply. |
| `upkeep` | int | No | `0` | Base energy credits charged per turn per owned **constructed** copy. Effective cost is `upkeep × FacilityEnergyUpkeep` modifiers (PureMultiplier; techs use `buildingFilter` to target All / BuildingId / Category). Continuous `GrantBuilding` expansions are not constructed and pay nothing. UI: `BuildingConfig_t::GetUpkeep()` (base), `BaseManager::GetBuildingUpkeepByType()` / `Faction::GetBuildingUpkeepByType()` (resolved). |
| `required_tech` | string | No | `""` | Tech that must be discovered before the building is available; omit or empty = always available |
| `allow_multiple` | bool | No | `false` | If true, a base may build more than one copy |
| `secret_project` | bool | No | `false` | If true, only one faction in the world may own this building |
| `orbital` | bool | No | `false` | If true, ownership counts are public to all factions (satellite census) |
| `stockpile` | bool | No | `false` | If true, this is a never-completing production item. After mineral support, leftover minerals convert via `MineralsConverted` StatModifiers on `effects` (output per mineral, rounded up per stat) during ResourceCollection, so income / research / growth see the credits this turn. Stockpile items cannot be constructed as facilities, but they remain selectable from the build menu. If the queue is empty, the base falls back to the **first available** stockpile in load order (tech gate applied). If none is available, the queue stays empty and excess minerals are wasted. |
| `effects` | Effect[] | if `stockpile` | `[]` | Structured list of gameplay effects (see below). On a stockpile item, yield is specified with `amount_source: "MineralsConverted"` (and optional extra StatModifiers on the same stats). These do not apply as a constructed building. |

A flat per-turn bonus (the old `nutrients_bonus`) is a `StatModifier` effect with `scope: "ThisBase"`. A per-improvement bonus (the old `improvement_bonuses`) is a `TileYieldModifier` effect with a `HasImprovement` selector — see Effect Types below.

### Load-time validation

The parser fails the load rather than substituting a default, so a setting either applies or the game does not start:

- **Unknown keys are rejected.** A typo'd `allow_multiples` is an error naming the building and the key, not a silently ignored line.
- **Wrong-typed values are rejected.** `"allow_multiple": "yes"` is an error, where it used to parse as `false`.
- **`mineral_cost` must be a non-negative integer.**
- **`upkeep` must be a non-negative integer.**
- **`secret_project` and `allow_multiple` are mutually exclusive.** A secret project is unique in the world, so "more than one copy" is unexpressible.
- **A `stockpile` item cannot have `mineral_cost`** (the field does not apply; cost is always 0). Combining it with upkeep, `secret_project`, `orbital`, or `allow_multiple` is rejected. `required_tech` is allowed.
- **A `stockpile` item requires at least one `StatModifier` with `amount_source: "MineralsConverted"`** on nutrients / energy / econ / labs / psych, `scope: ThisBase`, `op: Add`, `amount` > 0. Multiple modifiers on different stats credit multiple outputs. Extra StatModifiers on those stats (e.g. `AddPercent`) modify the resolved yield. `MineralsConverted` on a non-stockpile item is rejected.

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
| `persistence` | string | No | `"Continuous"` | When the effect applies (see Persistence below) |
| `condition` | string | No | `""` | Optional Lua expression; effect is suppressed when it evaluates to false |
| `unitFilter` | object | No | absent | Restricts which units receive the effect (`Domain` / `HasComponent` / `HasFlag`) |
| `buildingFilter` | object | No | absent (= all buildings) | Restricts which building types receive FacilityEnergyUpkeep (and similar) modifiers: `{ "kind": "All" }`, `{ "kind": "BuildingId", "building": "..." }`, or `{ "kind": "Category", "category": "grow" }` |
| `parameters` | object | No | `{}` | Key/value strings interpreted by the effect handler |

### Effect Types

| Value | Description |
|---|---|
| `GrantBuilding` | Instantly grants another building by ID (`parameters.building_id`). Continuous grants expand the target's effects only (no constructed copy, no maintenance). Instantaneous grants call `AddBuilding` and the real facility pays upkeep normally. |
| `GrantTech` | Instantly grants a technology by ID (`parameters.tech_id`) |
| `GrantUnit` | Spawns a unit (`parameters.unit_design_id`) |
| `StatModifier` | Adds or multiplies a named stat (`parameters.stat`, `parameters.amount`, `parameters.op`) |
| `RuleFlag` | Enables a named gameplay rule (`parameters.flag`) |
| `SocialEngineeringOverride` | Forces a social engineering value (`parameters.category`, `parameters.value`) |
| `DiplomaticModifier` | Adjusts diplomatic standing (`parameters.target_faction_id`, `parameters.value`) |
| `TileYieldModifier` | Modifies the yield of selected tiles (`parameters.resource`, `parameters.selector`, `parameters.amount`, `parameters.op`) — see below |
| `OrbitalAttack` | ASAT charge against other factions' `orbital` buildings (`parameters.chance`, `parameters.cooldown_turns`, `parameters.chance_of_destruction_on_fail`) |
| `InterceptAttempt` | Pre-combat intercept (`parameters.chance`, optional `cooldown_turns`, `chance_of_destruction_on_fail`; requires `unitFilter`) |

`amount`/`value` accept either a JSON number or a numeric string. `op` is one of `Add`, `AddPercent` (amount in percent points, e.g. `25` = +25%), `MultiplyGeometric` (factor form, e.g. `0.5`) — defaults to `Add`.

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

### Persistence

| Value | Description |
|---|---|
| `Continuous` (default) | Active as long as the building exists |
| `Instantaneous` | Fires once when the building is first completed |

`persistence` may be omitted entirely when `Continuous` — it's the default, so config files only need to write it for `Instantaneous` effects.

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

### Stockpile Energy

Never-completing production item. After mineral support, leftover minerals convert into energy at 0.5 per mineral (`ceil`). Selectable from the build menu; never constructed as a facility. `mineral_cost` is omitted — the field does not apply.

```json
{
  "id": "Stockpile_Energy",
  "name": "Stockpile Energy",
  "stockpile": true,
  "effects": [
    {
      "type": "StatModifier",
      "scope": "ThisBase",
      "parameters": {
        "stat": "energy",
        "amount": 0.5,
        "amount_source": "MineralsConverted",
        "op": "Add"
      }
    }
  ]
}
```

### Instantaneous grant

```json
{
  "id": "Merchant_Exchange",
  "name": "Merchant Exchange",
  "mineral_cost": 80,
  "upkeep": 2,
  "secret_project": true,
  "required_tech": "industrial_economics",
  "effects": [
    {
      "type": "GrantBuilding",
      "scope": "AllOwnerBases",
      "persistence": "Instantaneous",
      "parameters": { "building_id": "Energy_Bank" }
    }
  ]
}
```
