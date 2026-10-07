# SMAC fungus generation

Place world-gen fungus the way SMAC does: in contour bands of SMAC's lattice fractal, widened
or narrowed by the planet's native life setting. SMAC's maps then show their speckled, banded
fungus at SMAC's density (15%, 30% and 45% of tiles for rare, average and abundant native
life).

## Context

### What SMAC does

`world_build` (`0x5C86E0`) runs `world_fungus` (`0x5C3440`) after the continents, erosion,
shorelines, temperature and river sources, and before the landmarks and `world_rocky`.

- **Fractal** (`0x5C1E20` fills it, `0x5C1F20` samples it). A 16 × 16 lattice of random values
  0–15 that wraps on both axes, one cell every 256 units. `Sample(x, y)`:
  1. subtract 128 from both coordinates;
  2. cell = `(v >> 8) & 15`, fraction = `(v >> 3) & 31`;
  3. interpolate the four corners bilinearly in 32nds, then `>> 5`.

  The result runs 0–480.
- **Fungus value** (`0x5C1FC0` with shift 4, inlined in `world_fungus`): `a = Sample(16x, 16y)`,
  a broad field with a lattice point every 16 units, and `b = Sample(256x, 256y)`, per-tile
  detail. The value is `clamp((7 · (5a + 2b)) >> 8, 0, 100)`.
- **Bands.** `w = Fungus coefficient (alphax.txt, 1) × (native life − 1)`, so −1, 0 or +1. A
  tile gets fungus when its value lies in `[s − w, s + 5 + 2w]` for any `s` in 20, 40, 60.
  Outside every band, a tile in the first or last row gets it with chance ½.
- **Every tile.** Fungus is placed on land and sea alike. SMAC draws none in deep water.
  `map_wipe` (`0x591040`) clears rockiness, and `world_rocky` assigns it only after fungus, so
  `world_fungus`'s rocky test excludes nothing.
- **Coordinates.** SMAC's `x` steps by 2 along a row and `y` is the row. Our tiles use the same
  SMAC coordinates: tile `(x, y)` draws at `((x − camX)·½w, (y − camY)·½h)`, so the fractal
  samples the tile's own `(x, y)`.

### What we do today

- `PlaceFungus` grows separate patches of 1–16 tiles, which may not touch each other, until it
  covers `fungus.fraction` (0.29) of the tiles fungus can occupy.
- The native life setting (`native_life_levels.json`) does not reach world generation.

## Design

- **`SmacFractal`** (`game/map/SmacFractal.h`):
  - built from a 16 × 16 lattice, or drawn from the generator's RNG (uniform 0–15 per cell);
  - `Sample(x, y)` and `Value(x, y, fineShift)` as above;
  - fungus uses `fineShift` 4.
- **Placement** (`PlaceFungus` in `FungusGeneration`): one fractal per map. For every tile the
  Fungus entry can occupy (`CanBuildImprovement`), take the value at the tile's own `(x, y)`. Add
  fungus when it falls in a band. A tile on the first or last row (`y` 0 or height − 1) that
  misses every band gets fungus with chance `pole_row_chance`. Deep ocean keeps the fungus
  dormant through `suppress_terrain`, as today.
- **Config:**
  - `decoration.json` `fungus` becomes `{"band_starts": [20, 40, 60], "band_width": 5,
    "pole_row_chance": 0.5}`;
  - `fraction`, `min_patch_tiles`, `max_patch_tiles` and `patch_size_skew` go, with the
    patch-growth code;
  - each `native_life_levels.json` level gets `fungus_band_widening`: rare −1, normal 0,
    abundant 1;
  - a band spans `[start − widening, start + band_width + 2 · widening]`.
- **Native life to world gen.** `WorldGenerator::Generate` takes the session's
  `NativeLifeLevel_t`, which `Engine` already resolves with `RequireForSession`, and hands
  `fungusBandWidening` to `PlaceFungus`.

## Changes

1. `SmacFractal` (`include/game/map/SmacFractal.h`, `src/game/map/SmacFractal.cpp`).
2. `FungusGeneration`: band placement replaces patch growth. `PlaceFungus(rWorld, rConfig,
   widening, rFungus, rRng)`.
3. `WorldGenDecorationConfig.h` and the parser: the new `fungus` keys, all required. The parser
   checks that `band_starts` is non-empty and within 0–100, `band_width` ≥ 0 and
   `pole_row_chance` in [0, 1].
4. `NativeLifeLevelConfig`: `fungusBandWidening`, a required key.
5. `WorldGenerator::Generate` and `Engine::StartNewGame_`: pass the native life level.
6. Configs:
   - `config/worldGen/decoration.json`, `config/native_life_levels.json`;
   - `tests/fixtures/worldGen/decoration.json` and the native life fixture.
7. Tests (`FungusGenerationTests`, a new `SmacFractalTests`):
   - **Sampling:** a lattice point samples 32 × its value; halfway between two points samples
     their mean; the lattice wraps every 16 cells.
   - **Value formula:** constant lattices pin it; a lattice of 4 gives 24 and one of 7 gives 42.
   - **Bands:**
     - a constant lattice of 4 covers every tile at widening 0 and none at −1;
     - 7 covers every tile at all three widenings;
     - 11 (67) covers every tile only at +1;
     - 3 (18) covers none.
   - **Pole rows:** with an out-of-band lattice, chance 1 covers exactly the first and last
     rows and chance 0 covers nothing.
   - **Coordinates:** each tile follows the value at its own `(x, y)`.
   - **Coverage:**
     - land and sea get fungus alike;
     - a tile the Fungus entry cannot occupy stays clear;
     - deep ocean stores the fungus dormant.
   - **Density:** over fixed seeds on a 112 × 56 map, widening −1, 0 and +1 cover 15%, 30% and
     45% of tiles, within 3 points.
   - **Removed:** the patch tests (fraction, patch size, skew) go with the behavior they pinned.
   - **Config:** `WorldGenPipelineTests` and the parser tests move to the new keys.
8. Docs:
   - `map-system.md`: the pipeline diagram and the "Fungus before landmarks" note;
   - a new `docs/thinker/smac-world-generation.md` for `world_build`'s stage order, the fractal
     and `world_fungus`;
   - `world-map-followups.md`: its map-content bullet keeps only the aquifer fraction.

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`, then screenshot a new map next to the SMAC window. The fungus should show
  the same speckled bands and cover about 30% of the land at normal native life.
