# SMAC world generation (`world_build`)

Stock map build order, the fungus fractal, and rockiness (`minerals_at`), reverse-engineered
from `terranx.exe` (GOG build, image base `0x400000`) with Thinker's names
(`induktio/thinker`, `engine.cpp`). Our fungus port is `SmacFractal` / `PlaceFungus`. Our
rockiness stage keeps the same pipeline slot as `world_rocky` but uses configurable
elevation-weight bands in `decoration.json` (`PlaceRockiness`), not a bit-identical
`minerals_at` port — see [map-system.md](../architecture/map-system.md).

## `world_build` stage order (`0x5C86E0`)

Relevant fungus- and rockiness-adjacent stages:

1. Continents / elevation
2. Erosion
3. Shorelines
4. Temperature
5. River sources
6. **`world_fungus` (`0x5C3440`)**
7. Landmarks
8. Climate (`world_climate` / rainfall)
9. **`world_rocky` (`0x5C32A0`)**

`map_wipe` (`0x591040`) clears rockiness before the build, and `world_rocky` runs after fungus
and climate, so `world_fungus`'s rocky test excludes nothing on a fresh map. Fungus is written
on land and sea alike; SMAC simply draws none in deep water. Our pipeline keeps fungus before
landmarks with no rocky gate, and stamps rockiness after rainfall.

## Lattice fractal

Fill (`0x5C1E20`) builds a 16 × 16 lattice of random bytes 0–15 (stored with pitch 19 so
neighbors wrap). Sample (`0x5C1F20`):

1. Subtract 128 from both coordinates.
2. Cell = `(v >> 8) & 15`, fraction = `(v >> 3) & 31`.
3. Bilinear interpolate the four corners in 32nds, then `>> 5`.

The sample runs 0–480. A constant lattice of value `V` samples `32 · V` everywhere; a point
exactly on a lattice cell samples `32 ·` that cell.

Value (`0x5C1FC0`, shift argument `fineShift`):

- `a = Sample(16x, 16y)`
- `b = Sample((16x) << fineShift, (16y) << fineShift)`
- result = `clamp((7 · (5a + 2b)) >> 8, 0, 100)`

Fungus uses `fineShift` 4, so `b = Sample(256x, 256y)`. Constant lattices pin the fungus
value: 4 → 24, 7 → 42, 11 → 67, 3 → 18.

## `world_fungus` bands

`w = WorldBuilder.fungus (alphax, 1) × (MapNativeLifeForms − 1)`, so −1 / 0 / +1 for rare /
average / abundant. A tile gets `BIT_FUNGUS` when its value lies in
`[s − w, s + 5 + 2w]` for any `s` in 20, 40, 60. Outside every band, a tile on the first or
last row still gets fungus with chance ½ (`map_rand.get(2)`).

SMAC walks packed coordinates (`x` steps by 2 along a row). Our tiles already use those
coordinates, so the fractal samples each tile's own `(x, y)`.

At those widenings the bands cover about 15%, 30% and 45% of tiles.

## `world_rocky` and `minerals_at`

`world_rocky` (`0x5C32A0`) walks staggered columns (`x = y & 1; x += 2`). For each tile it
calls `minerals_at` (`0x591F00`) and `rocky_set`s the result (0 Flat, 1 Rolling, 2 Rocky). It
also stamps nutrient/mineral/energy bonus bits from rainfall and landmarks; those bonus writes
are out of scope for our generator.

`minerals_at` (Thinker `mod_minerals_at` asserts equal to the binary):

1. Pole early-out: `y == 0` or `y == MapAreaY - 1` → Rocky.
2. Else, with `avg = (x + y) / 2`, `MapRandomSeed`, and the tile's `alt_level`:
   - `val1 = 3 · ((x − avg) / 2) + 2 · (avg / 2) + MapRandomSeed`
   - `val2 = (val1 − 2 · ((x − avg) & 1) − (avg & 1)) & 3`
   - `val3 = |alt − 3| − (alt < 3)` (distance from shore, ocean side reduced by one)
3. A jump table on `val3` (0…3) then `val2` picks 0 / 1 / 2; `val3 > 3` returns `~val2 & 1`.
   When `val3 == 0` and `val2 == 2`, bit 4 of `val1` chooses Rocky vs Rolling.

`MapRandomSeed` is the session seed SMAC stores at `0x949878` (forced non-zero at world
build). Altitude bands are SMAC `TerrainAltitude` (ocean trench … four above sea).

Our `PlaceRockiness` does not call this hash. It draws from `decoration.json` pole weights and
an ordered list of elevation-above-ocean weight bands (last band unbounded above), after
rainfall, so landmark-sculpted elevations are visible.
