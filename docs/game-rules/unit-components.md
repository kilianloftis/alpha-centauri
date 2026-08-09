# Unit Components

Unit components are defined as JSON arrays inside `config/unit_components/`. Every `.json` file in that directory is loaded automatically and merged into a single registry, so components can be split across as many files as desired. The conventional split is one file per component type (`chassis.json`, `weapons.json`, `armour.json`, `reactors.json`, `abilities.json`), but mods can add new files freely without touching the originals.

A unit is assembled from exactly one chassis, one weapon, one armour, one reactor, and up to two abilities. Stats are resolved by aggregating contributions from all equipped components.

---

## Required Fields

| Field | Type | Description |
|---|---|---|
| `id` | string | Unique identifier used to reference this component in code and other configs |
| `name` | string | Display name shown in the UI |
| `type` | string | Component slot: `chassis`, `weapon`, `armour`, `reactor`, or `ability` |
| `required_tech` | string | Tech ID that must be researched to unlock this component. Empty string means always available |
| `mineral_cost` | int | Flat mineral cost this component adds to the unit's construction cost |

---

## `stats` Block

Numeric stat contributions. The final value of any stat is resolved by summing contributions from all equipped components using the formula:

```
final = Σ(base) × (1 + Σ(additive_mult)) × Π(geometric_mult)
```

- **`base`** — Flat value added to the stat pool (default: `0`)
- **`additive_mult`** — Fraction of the base added on top, summed with other additive multipliers before applying (default: `0`). E.g. `0.5` means +50%.
- **`geometric_mult`** — Multiplied together across all components (default: `1.0`). E.g. `1.25` means ×1.25.

Any combination of the three fields can be omitted; omitted fields use their defaults.

### Example: stacking multipliers

A weapon with `"attack": { "base": 10 }` combined with a reactor providing `"attack": { "additive_mult": 0.5 }` and an ability providing `"attack": { "geometric_mult": 1.2 }` resolves to:

```
10 × (1 + 0.5) × 1.2 = 18
```

### Known stat keys

| Key | Source component | Notes |
|---|---|---|
| `attack` | weapon | Base offensive value |
| `defense` | armour | Base defensive value |
| `movement` | chassis | Movement points per turn |
| `hit_points` | reactor | Unit HP |
| `disengage_chance` | chassis | % chance to disengage from combat |
| `turns_of_fuel` | chassis | Turns of fuel; max fuel = turns × movement. Omit or `0` = unlimited (no fuel tracking) |
| `damage_from_out_of_fuel` | chassis | Percent of max HP applied when ending a turn at 0 fuel away from a refuel site |
| `cargo_capacity` | chassis | Number of units that can be transported |
| `difficult_terrain_cost` | chassis | Movement cost over difficult terrain |
| `cost_multiplier` | reactor | Applied as a geometric multiplier to the total unit mineral cost |

Any component can contribute to any stat key, not just its "source" component.

---

## `flags` Block

Boolean properties. A flag is `true` on the unit if **any** equipped component has it set to `true`.

### Known flag keys

| Key | Description |
|---|---|
| `flight` | Unit can fly, ignoring terrain movement costs |
| `single_use` | Unit is expended on use |
| `attacking_ends_turn` | Attack spends all remaining moves (Needlejet: one strike per turn) |

New flags can be added freely in JSON without any code changes.

---

## `bonus_tables` Block

Named tables of keyed float bonuses. Values for the same key are **summed** across all equipped components.

```json
"bonus_tables": {
  "terrain_attack": {
    "Forest": 25,
    "Hill": 10
  },
  "unit_type_attack": {
    "Armour": 50
  }
}
```

### Known table keys

| Key | Description |
|---|---|
| `terrain_attack` | Attack bonus (%) when fighting on the named terrain type |
| `unit_type_attack` | Attack bonus (%) when fighting the named unit type |

New tables can be added freely in JSON without any code changes.

---

## Minimal Entry Example

```json
{
  "id": "My_Weapon",
  "name": "My Weapon",
  "type": "weapon",
  "required_tech": "",
  "mineral_cost": 5,
  "stats": {
    "attack": { "base": 4 }
  },
  "flags": {},
  "bonus_tables": {}
}
```

## Full Entry Example

```json
{
  "id": "Marine",
  "name": "Marine",
  "type": "chassis",
  "required_tech": "Doctrine_Mobility",
  "mineral_cost": 15,
  "stats": {
    "movement":              { "base": 2 },
    "difficult_terrain_cost":{ "base": 2 },
    "disengage_chance":      { "base": 10 },
    "attack":                { "additive_mult": 0.25 }
  },
  "flags": {
    "flight": false,
    "single_use": false
  },
  "bonus_tables": {
    "terrain_attack": {
      "Ocean": 50
    },
    "unit_type_attack": {
      "Ship": 25
    }
  }
}
```
