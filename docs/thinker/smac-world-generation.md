# SMAC world generation (`world_build`)

Stock map build order and the fungus fractal, reverse-engineered from `terranx.exe`
(GOG build, image base `0x400000`) with Thinker's names (`induktio/thinker`, `engine.cpp`).
Our port is in [map-system.md](../architecture/map-system.md) and `SmacFractal` /
`PlaceFungus`.

## `world_build` stage order (`0x5C86E0`)

Relevant fungus-adjacent stages:

1. Continents / elevation
2. Erosion
3. Shorelines
4. Temperature
5. River sources
6. **`world_fungus` (`0x5C3440`)**
7. Landmarks
8. **`world_rocky` (`0x5C32A0`)**

`map_wipe` (`0x591040`) clears rockiness before the build, and `world_rocky` runs after fungus,
so `world_fungus`'s rocky test excludes nothing on a fresh map. Fungus is written on land and
sea alike; SMAC simply draws none in deep water.

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
