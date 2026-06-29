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
| `mineral_cost` | int | No | `0` | Minerals required to construct |
| `required_techs` | string[] | No | `[]` | Building is available when **any** listed tech is discovered |
| `allow_multiple` | bool | No | `false` | If true, a base may build more than one copy |
| `secret_project` | bool | No | `false` | If true, only one faction in the world may own this building |
| `nutrients_bonus` | int | No | `0` | Flat nutrients added to this base each turn |
| `improvement_bonuses` | object | No | `{}` | Per-improvement nutrient bonuses (see below) |
| `effects` | Effect[] | No | `[]` | Structured list of gameplay effects (see below) |

### `improvement_bonuses`

Keys are improvement type names (e.g. `"Farm"`, `"Condenser"`). Each value is an object with a `nutrients` int.

```json
"improvement_bonuses": {
  "Farm":      { "nutrients": 1 },
  "Condenser": { "nutrients": 2 }
}
```

---

## Effects

Each entry in `effects` describes a single gameplay effect applied when the building is present.

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `type` | string | Yes | — | Effect category (see Types below) |
| `scope` | string | Yes | — | Who is affected (see Scopes below) |
| `persistence` | string | No | `"Continuous"` | When the effect applies (see Persistence below) |
| `condition` | string | No | `""` | Optional Lua expression; effect is suppressed when it evaluates to false |
| `parameters` | object | No | `{}` | Key/value strings interpreted by the effect handler |

### Effect Types

| Value | Description |
|---|---|
| `GrantBuilding` | Instantly grants another building by ID (`parameters.building_id`) |
| `GrantTech` | Instantly grants a technology by ID (`parameters.tech_id`) |
| `GrantUnit` | Spawns a unit (`parameters.unit_design_id`) |
| `StatModifier` | Adds or multiplies a named stat (`parameters.stat`, `parameters.amount`) |
| `RuleFlag` | Enables a named gameplay rule (`parameters.flag`) |
| `SocialEngineeringOverride` | Forces a social engineering value (`parameters.category`, `parameters.value`) |
| `DiplomaticModifier` | Adjusts diplomatic standing (`parameters.target`, `parameters.amount`) |

### Scopes

| Value | Description |
|---|---|
| `ThisBase` | Only the base that built this building |
| `AllOwnerBases` | Every base the owning faction controls |
| `FactionUnits` | Every unit the owning faction controls |
| `FactionGlobal` | A faction-wide capability, not tied to a specific base or unit |
| `WorldGlobal` | Affects every faction, not just the owner |

### Persistence

| Value | Description |
|---|---|
| `Continuous` | Active as long as the building exists |
| `Instantaneous` | Fires once when the building is first completed |

---

## Examples

### Regular building

```json
{
  "id": "Recycling_Tanks",
  "name": "Recycling Tanks",
  "mineral_cost": 5,
  "required_techs": ["ecology"],
  "effects": [
    {
      "type": "StatModifier",
      "scope": "ThisBase",
      "persistence": "Continuous",
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
  "secret_project": true,
  "required_techs": ["biogenetics"],
  "effects": [
    {
      "type": "RuleFlag",
      "scope": "FactionGlobal",
      "persistence": "Continuous",
      "parameters": { "flag": "population_boom" }
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
  "secret_project": true,
  "required_techs": ["industrial_economics"],
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
