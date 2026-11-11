# SMAC fauna boat spawn sites

Align planet-fauna boat placement with `alien_fauna` (`0x522180`): accept **land or sea
fungus**, or **empty ocean** in a water body of at least 50 tiles. Hull picks must not
require sea terrain when the site is land fungus.

## Target eligibility

Up to 1000 random tiles (unchanged). Accept a tile when either:

1. **Fungus** — tile has fungus (land or water). Confirm sea fungus shelf gate against
   `is_fungus` in disassembly while implementing; document the address in terranx-map.
2. **Empty ocean** — water, no units on tile, and water-region tile count ≥
   `boat_empty_ocean_min_region_tiles` (required config, shipping **50**).

Drop the current “must be water” gate in
[`FindBoatSpawnTile_`](src/game/natives/PlanetFaunaMutations.cpp).

## Region sizes

[`RegionCache`](include/game/map/RegionCache.h) already assigns region ids. Add
`TileCountInRegion(const Tile&)` (histogram built in `Validate_` from `AssignRegions`).
Boat empty-ocean branch reads that count; fungus branch ignores size.

## Rules / mutations / config

- New `PlanetFaunaRules::AcceptsBoatSpawnSite(const Tile&, const WorldMap&)` (or equivalent)
  owns the predicate; `FindBoatSpawnTile_` only samples and calls it.
- Required key on native-life / fauna config: `boat_empty_ocean_min_region_tiles: 50` in
  shipping + fixtures (parser requires it).
- **Hull / escort pick:** for boat hull (and escort) spawn, pick by role **without**
  `CanEnterTileTerrain` / domain tile filter (pass `nullptr` domain tile), matching SMAC’s
  no triad check before `veh_init`. Cargo already picks without a domain tile.
- `CreateUnit` stays without placement validation for these gifts.

## Docs

- [`docs/thinker/terranx-map.md`](docs/thinker/terranx-map.md) — replace the “our FindBoatSpawnTile_
  keeps only water…” note with the implemented predicate (+ confirmed `is_fungus` detail).
- [`docs/architecture/native-units-system.md`](docs/architecture/native-units-system.md) —
  boat sites and hull pick without enter-terrain gate.
- [`docs/architecture/map-system.md`](docs/architecture/map-system.md) — region tile counts on
  `RegionCache`.

## Tests

Fixture-owned maps only:

1. Land fungus only eligible site → Isle/Sealurk spawns on that land tile.
2. Connected water &lt; 50 tiles, empty, no fungus → no boat hull.
3. Connected water ≥ 50 tiles, empty → boat can spawn.
4. Region histogram unit test: two ponds, counts match flood sizes.
5. Keep existing sea-fungus embark/cargo test; adjust comments so it is not mistaken for full
   site parity.

## Files

- `PlanetFaunaRules` (+ header), `PlanetFaunaMutations.cpp`
- `RegionCache` / `RegionRules` as needed for counts
- Native life config + parser + `config/native_life.json` (or the file that owns fauna gates)
- `PlanetFaunaTests.cpp`, region tests
- Docs above
