# SMAC fauna boat spawn sites

Align empty-ocean boat placement with `alien_fauna` (`0x522180`): water body of at least
50 tiles. Sea fungus sites stay eligible. Land fungus is a deliberate deviation (boats stay
on water; see `config/deviations/README.md`).

## Target eligibility

Up to 1000 random tiles (unchanged). Accept a tile when either:

1. **Sea fungus** — water tile with fungus, not deep ocean (SMAC `is_fungus` shelf gate at
   `0x5226b3`; land fungus is out of scope here).
2. **Empty ocean** — water, no units on tile, and water-region tile count ≥
   `boat_empty_ocean_min_region_tiles` (required config, shipping **50**).

[`FindBoatSpawnTile_`](src/game/natives/PlanetFaunaMutations.cpp) samples and calls the
predicate; it does not own the water/fungus/region branches.

## Region sizes

[`RegionCache`](include/game/map/RegionCache.h) already assigns region ids. Add
`TileCountInRegion(const Tile&)` (histogram built in `Validate_` from `AssignRegions`).
Boat empty-ocean branch reads that count; fungus branch ignores size.

## Rules / mutations / config

- New `PlanetFaunaRules::AcceptsBoatSpawnSite(const Tile&, const WorldMap&)` (or equivalent)
  owns the predicate; `FindBoatSpawnTile_` only samples and calls it.
- Required key on native-life / fauna config: `boat_empty_ocean_min_region_tiles: 50` in
  shipping + fixtures (parser requires it).
- **Hull / escort pick:** keep `CanEnterTileTerrain` (pass the site as domain tile). Cargo
  still picks without a domain tile.
- `CreateUnit` stays without placement validation for these gifts.

## Docs

- [`docs/thinker/terranx-map.md`](docs/thinker/terranx-map.md) — confirmed `is_fungus` detail;
  note our sea-only site + domain-gated hull pick and point at the deviation entry.
- [`docs/architecture/native-units-system.md`](docs/architecture/native-units-system.md) —
  boat sites (sea fungus / large empty ocean) and hull domain filter.
- [`docs/architecture/map-system.md`](docs/architecture/map-system.md) — region tile counts on
  `RegionCache`.
- [`config/deviations/README.md`](../../config/deviations/README.md) — “Planet fauna boats stay
  on water”.

## Tests

Fixture-owned maps only:

1. Land fungus only → not an accepted site; no boat hull.
2. Connected water &lt; 50 tiles, empty, no fungus → no boat hull.
3. Connected water ≥ 50 tiles, empty → boat can spawn.
4. Region histogram unit test: two ponds, counts match flood sizes.
5. Keep existing sea-fungus embark/cargo test.

## Files

- `PlanetFaunaRules` (+ header), `PlanetFaunaMutations.cpp`
- `RegionCache` / `RegionRules` as needed for counts
- Native life config + parser + `config/native_life.json` (or the file that owns fauna gates)
- `PlanetFaunaTests.cpp`, region tests
- Docs above
