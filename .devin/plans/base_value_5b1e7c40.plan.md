---
name: Base value
overview: SMAC's value_of_base becomes BaseValue, a diplomacy rule over a configured calculator. Its inputs need three shared pieces. Tile regions are SMAC's land and water bodies (RegionRules, RegionCache). Headquarters distance is the binary's vulnerable, shared with probes. The binary's ring-ordered offset table drives the neighbour scan. Gift standing's base branch (gift_standing_a8529f4f) is the first caller and lands next. Base swap, base sale and threatened cession stay recorded until AI evaluation exists.
todos:
  - id: re-record
    content: "terranx-map.md: Regions, Base value, Base swap, Base gift, Threatened cession; deviations: base swap, BASEGIFT reply, region cap bugs; Submissive caption's 0x93FA98"
    status: completed
  - id: regions
    content: RegionRules AssignRegions (uncapped); RegionCache on WorldMap, validated against the appearance revision
    status: completed
  - id: hq-distance
    content: BaseRules HeadquartersDistance with map_rules.json missing_headquarters_distance; ProbeRules calls it and drops its copy
    status: completed
  - id: ring-table
    content: "MapUtils: the binary's TableOffset as k_TableOffsets, and ForEachTileInRings"
    status: completed
  - id: base-value
    content: diplomacy.json base_value block + parser; DiplomacyCalculator::BaseValue; BaseValueRules BaseValue
    status: completed
  - id: deviations-docs
    content: Deviations entries below; map-system.md regions; diplomacy-system.md base value
    status: completed
  - id: tests-verify
    content: Parsers, regions, cache, ring table, HQ distance, calculator, rule; ./bd build; ./bd test; LastTest.log; boot the game
    status: completed
isProject: false
---

# Base value

## Context

SMAC weighs a base in diplomacy with `value_of_base(base, faction, other, fear, hostile)`
(`0x54CB50`). `faction` is the AI weighing it and `other` the human it deals with. Three
functions call it: `base_swap` (a base for a base, energy or research), `make_gift` (a base
given away) and `threaten` (a base demanded under threat). [terranx-map.md](docs/thinker/terranx-map.md)
records all of them under "Diplomacy standing and talk", and records regions under "Climate",
"Regions".

Our AI accepts every proposal (`EvaluateResponse_`), so only the gift has a place to call the
value. [gift_standing_a8529f4f.plan.md](.devin/plans/gift_standing_a8529f4f.plan.md) values base
gifts with `BaseValue` and lands after this plan; until then the rule's only callers are tests.

## The rule

`BaseValue` follows the map's "Base value" steps 1–8. Steps 9–10 and the `fear` and `hostile`
inputs are deviations (below).

1. `n` = headquarters distance from the evaluator's HQ to the base.
2. The ring scan adjusts `n` by nearby bases.
3. `(max treasury + 1000) / (n + 4) × max(pop, 3)`, at least 100.
4. The ungarrisoned and region-holding multipliers apply.
5. The facility, project and garrison costs are added.

### Regions

New `RegionRules` (`include/game/map/`): `std::vector<int> AssignRegions(const WorldMap&)`, one
region id per tile in the map's tile order. It follows the map's "Regions" without the cap:

- A tile on rows 1 to height − 2 shares a region with its same-surface (`IsWater`) neighbours
  among the binary's 8 on those rows. Two bodies that touch only through the first or last row
  stay apart, as the binary's flood never expands from those rows.
- Each pole (the first row, and the last) takes one id per surface ("Poles are separate
  regions").
- Ids are opaque ints; land and water never share one. Every body keeps its own id
  ("Region cap empties the rest of the map").

New `RegionCache` (`include/game/map/`), owned by `WorldMap` and returned by
`const RegionCache& GetRegions() const`:

- `int RegionOf(const Tile&) const`.
- It memoises `AssignRegions` against `GetAppearanceRevision()`, like `CommerceCache`. Every
  land↔water change goes through `SetElevation`, which bumps that revision, and SMAC reruns the
  scan after every such change (`world_climate`).

### Headquarters distance

`config/map_rules.json` and its test fixture gain `"missing_headquarters_distance": 12`, which
`MapRulesConfigParser` requires at 1 or more. `MapRulesConfig_t` gains
`missingHeadquartersDistance`.

`BaseRules` gains `int HeadquartersDistance(const Faction&, const Tile&)`: the binary's
`vulnerable`. It is `TabletopDiagonalDistance` to the faction's headquarters, or
`missingHeadquartersDistance` when it has none. `ProbeRules` calls it from both cost quotes and
loses `DistanceToHeadquarters_` and `k_defaultHqDistance`. Probe costs do not change.

### Ring scan

`MapUtils.h` gains `k_TableOffsets`: the binary's `TableOffsetX/Y` (289 map offsets, rings 0–8,
Thinker's `path.h` order), with a one-line pointer to the map. Also
`ForEachTileInRings(rOrigin, rMap, radius, fn)`: entries 1 to `(2 × radius + 1)² − 1` in table
order, x wrapping as `GetTile` does, rows off the map skipped. `fn` returns whether to keep
going.

### Config

`config/diplomacy.json` and `tests/fixtures/diplomacy.json` gain the block below.
`DiplomacyConfigParser` requires it. `scan_radius` must be 1–8; every divisor, ratio and
`minerals_per_row` must be at least 1.

```json
"base_value": {
  "scan_radius": 6,
  "credits_offset": 1000,
  "distance_offset": 4,
  "min_population": 3,
  "minimum": 100,
  "ungarrisoned_divisor": 2,
  "counterparty_absent_multiplier": 2,
  "lone_base_multiplier": 2,
  "outnumbered_ratio": 5,
  "outnumbered_divisor": 2,
  "minerals_per_row": 10,
  "project_rows_multiplier": 25,
  "unit_rows_multiplier": 2
}
```

`DiplomacyConfig_t` gains `BaseValueConfig_t baseValue`.

SMAC's costs are rows, and ours are minerals. Each cost term is summed in minerals, then divided
once by `minerals_per_row`. This equals the binary whenever a cost is a whole number of rows.

### Calculator

`DiplomacyCalculator::BaseValue(const BaseValueInputs_t&) const`, with integer steps in the
binary's order. Every division truncates.

`BaseValueInputs_t` holds:

- `distance`: `n` after the scan;
- `credits`: the larger treasury;
- `population`;
- `bGarrisoned`;
- `evaluatorRegionBases` and `counterpartyRegionBases`;
- `facilityMinerals`: built facilities, in registry order;
- `projectMinerals` and `evaluatorUnitMinerals`: sums.

1. `v = (credits + credits_offset) / (distance + distance_offset) × max(population,
   min_population)`, then `max(v, minimum)`.
2. Not garrisoned: `v /= ungarrisoned_divisor`.
3. Counterparty has no base in the region: `× counterparty_absent_multiplier`. Evaluator has
   exactly one and the counterparty at most one: `× lone_base_multiplier`.
4. Both have bases there and the counterparty has at least `outnumbered_ratio` × the
   evaluator's: `v /= outnumbered_divisor`.
5. `+ (Σ k × facilityMinerals[k]) / minerals_per_row`, k counting from 1.
6. `+ project_rows_multiplier × projectMinerals / minerals_per_row`.
7. `+ unit_rows_multiplier × evaluatorUnitMinerals / minerals_per_row`.

### BaseValueRules

New `include/game/diplomacy/BaseValueRules.h`:
`int BaseValue(const BaseManager& rBase, const Faction& rEvaluator, const Faction& rCounterparty)`.
It reads the game through `CurrentGameState()`. Each input is a named step in the `.cpp`.

- **Distance:** `HeadquartersDistance(rEvaluator, base tile)`. Zero means the base is the
  evaluator's headquarters, the binary's −1. Every caller in the binary refuses that base first,
  so `BaseValue` throws.
- **Scan** (`ForEachTileInRings`, `scan_radius`), using `RegionOf` against the base's region. At
  each tile with a base, first match wins:
  - the evaluator's base: −1 when in the same region, then stop;
  - another region: skip;
  - the counterparty's: +1;
  - anyone else's: stop.
- **Credits:** the larger of the two factions' energy.
- **Garrison:** any unit on the base tile.
- **Region counts:** each faction's bases whose tile is in the base's region, counted from
  current ownership.
- **Facilities:** walk `BuildingRegistry::GetAll()` in load order and keep those the base holds
  that are neither secret projects nor `orbital`. `buildings.json` follows `alphax.txt`
  `#FACILITIES`, so load order is SMAC's id order.
- **Projects:** the secret projects the base holds.
- **Units:** the evaluator's units on the base tile, by `GetDesign().GetBaseCost()`.
- A one-line TODO names the inputs we lack (fear, hostility, objective, Believers), citing the
  map.

## Deviations

[config/deviations/README.md](config/deviations/README.md):

- **New "Base value skips the AI's attitude and scenario inputs":**
  - Original: `fear` halves and `hostile` quadruples the value; `diplomacy_check` fills both.
    A scenario objective quadruples it, and a `BELIEVE` evaluator doubles it ("Base value").
  - Ours: none applies. There is no attitude model and there are no scenarios, and the
    Believers do not ship.
  - Where: `BaseValue` (`BaseValueRules`).
  - Missing: the RE of `diplomacy_check`'s fear, hostility and attack chance, and the inputs;
    scenario objectives; a faction-scope `BaseValue` stat with the Believers.
- **New "Region base counts are live `[Ours]`":**
  - Original: `region_total_bases` is recounted at each faction's upkeep and moved by
    `base_init` / `base_kill`, so a capture or cession counts for its old owner until then
    ("Regions").
  - Ours: counted from current ownership on every query.
  - Where: `BaseValue`'s region counts.
  - Missing: the original version and a switch.
- **"Region cap empties the rest of the map `[Bug]`":** Ours becomes "no cap: every land and
  water body keeps its own region". Where: `AssignRegions` (`RegionRules`). Missing: the original
  version (the 62-per-surface cap with its empty-slot search) and a switch.

## Docs

- [map-system.md](docs/architecture/map-system.md): `RegionRules` and `RegionCache` in the diagram
  and a short "Regions" section (surfaces, polar rows, revision).
- [diplomacy-system.md](docs/architecture/diplomacy-system.md): a "Base value" subsection (the
  inputs, the scan, the calculator steps) and `BaseValueRules` in the diagram.

## Tests

Fixtures own every number; neither a test nor a rule reads `config/`.

- **`MapRulesConfigParser`:** `missing_headquarters_distance` is required; 0 is rejected.
- **`DiplomacyConfigParserTests`:** `base_value` is required; `scan_radius` 0 and 9 are rejected.
- **New `RegionRulesTests`** (maps built in the test):
  - two islands get different ids; the sea between them shares one; land and water never share;
  - land touching only at a lattice corner is one region;
  - each pole takes one id per surface, and the two poles differ;
  - two land bodies joined only through the first row are different regions.
- **New `RegionCacheTests`:** raising the water tile between two islands joins them on the next
  `RegionOf`.
- **`MapUtils`:** ring r of `k_TableOffsets` holds 8r offsets, each at Chebyshev distance r.
  Ring 1 runs (1,−1), (2,0), (1,1), (0,2), (−1,1), (−2,0), (−1,−1), (0,−2).
  `ForEachTileInRings` stops when `fn` returns false.
- **`BaseRules`:** `HeadquartersDistance` measures to the headquarters, and the fixture's
  fallback applies without one. Existing probe cost tests still pass unchanged.
- **`DiplomacyCalculatorTests` `BaseValue`:**
  - the minimum and the population floor;
  - the ungarrisoned halving truncates;
  - the absent ×2 and the lone ×2 stack;
  - the outnumbered halving at exactly `ratio × F`, and none at one below;
  - two facilities [a, b] add (a + 2b) / `minerals_per_row`;
  - projects and units.
- **New `BaseValueRulesTests`** (fixture session):
  - a counterparty base in the same region lowers the value; one in another region does not;
  - an evaluator base in the same region raises it and hides a counterparty base further out;
  - an evaluator base in another region stops the scan without changing `n`;
  - a third faction's base in the same region stops the scan;
  - the evaluator's headquarters throws;
  - another faction's unit garrisons, while only the evaluator's units add their cost;
  - facilities weigh by registry order, and an orbital building adds nothing.

## Verification

`./bd build`, `./bd test`, and check `build/Testing/Temporary/LastTest.log` for the new cases.
Boot the game to confirm `map_rules.json` and `diplomacy.json` load.
