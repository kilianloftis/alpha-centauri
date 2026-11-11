# SMAC rockiness (`minerals_at`)

Replace per-tile weighted float lottery with SMAC `minerals_at` / `world_rocky`: deterministic
0/1/2 from `(x, y)`, map seed, and altitude band. Run after landmarks and climate, not before
fungus.

Vanilla only — Thinker `mod_minerals_at` is the lead (`assert` against the binary); confirm
pole early-out and jump table at `0x591F00`.

## Target behaviour

- For each tile (SMAC staggered column scan `x = y&1; x += 2` over the map):
  - Pole rows `y == 0` or `y == height - 1` → Rocky.
  - Else `minerals_at(x, y, altLevel, mapRandomSeed)` → Flat / Rolling / Rocky.
- `mapRandomSeed` is the **session seed** (same source SMAC stores in `MapRandomSeed`), not a
  fresh RNG draw per tile.
- Altitude level from tile elevation via existing
  [`AltitudeBand`](include/game/worldgen/MoistureGeneration.h) / SMAC `alt_level` mapping.
- Pipeline: move rockiness to **after** landmarks and `RecomputeRainfall` / moisture (SMAC
  `world_rocky` after climate). Fungus stays before landmarks and does **not** gain a rocky
  gate.

Bonus bits that vanilla stamps inside `world_rocky` are out of scope; this plan is rockiness
enum only. `PlaceTileBonuses` can remain after rockiness.

## Implementation

- New `MineralsAt` (or `SmacRockiness`) free functions under `game/worldgen/`:
  `Rockiness_t MineralsAt(int x, int y, int altLevel, unsigned mapRandomSeed)`.
- [`WorldGenerator`](src/game/worldgen/WorldGenerator.cpp): relocate `GenerateRockiness_`; pass
  session seed; stop calling `SampleRockiness` / `RandomFloat_` for this stage.
- Delete decoration `rockiness` weights (`RockinessDecorationConfig_t`,
  `RockinessWeights_t`, `WeightsForLevel`, `SampleRockiness`) and the
  `decoration.json` / fixture `rockiness` blocks. `ErosiveForces_t` may remain in map settings
  but no longer drives rockiness (document; wiring erosive into elevation is a separate
  follow-up).

## Docs

- [`docs/thinker/smac-world-generation.md`](docs/thinker/smac-world-generation.md) — create or
  extend: `world_rocky` `0x5C32A0`, `minerals_at` `0x591F00`, stage order vs fungus/landmarks.
- [`docs/architecture/map-system.md`](docs/architecture/map-system.md) — pipeline diagram and
  remove weight-lottery wording.
- Cross-link from [`docs/plans/smac-fungus-generation.md`](docs/plans/smac-fungus-generation.md)
  (fungus before rockiness; no rocky filter).

## Tests

- Table-driven `MineralsAt` vectors: fixed `(x, y, seed, altLevel)` → rockiness; poles always
  Rocky. Prefer vectors checked against Thinker’s `mod_minerals_at` / binary.
- Pipeline determinism after reorder; landmark elevation changes affect rockiness on sculpted
  tiles.
- Replace [`WorldGeneratorRockinessTests`](tests/game/WorldGeneratorRockinessTests.cpp)
  weight-band cases; parser/fixtures no longer carry `rockiness` weights.

## Ordering

Ship **after or independently of** fungus fractal work. Fungus must not wait on rockiness.
Rockiness **must** run after final elevation for the tile (landmarks) and should use the same
alt bands rainfall uses.

## Files

- New worldgen minerals/rockiness sources + headers
- `WorldGenerator.cpp` / `.h`, decoration config + parser, `decoration.json` + fixtures
- `WorldGeneratorRockinessTests.cpp`, pipeline tests
- `map-system.md`, `smac-world-generation.md`
