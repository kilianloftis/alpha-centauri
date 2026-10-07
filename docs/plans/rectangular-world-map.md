# Rectangular world map

The isometric view turns the square grid 45° on screen, so the whole planet draws as a tilted
strip: the poles run diagonally, and so does the east–west wrap seam. Store the map the way
SMAC does instead, in rows of diamonds. The map is then a screen rectangle: the poles are its
top and bottom rows, and the wrap seam runs down its left and right edges. Movement, radii and
distances stay on the same square lattice. Only tile addressing and the map's outline change.

## Context

### What SMAC does

- A tile sits at `(x, y)` with `x + y` even. `y` is the screen row, 0 at the north edge. `x`
  counts half-tile columns, so a row holds `axis_x / 2` tiles and `x` wraps at `axis_x`.
  Thinker's `mapsq` stores tile `(x, y)` at index `x / 2 + y · (axis_x / 2)`.
- A tile's eight neighbors, NE first and clockwise: `(1, −1)`, `(2, 0)`, `(1, 1)`, `(0, 2)`,
  `(−1, 1)`, `(−2, 0)`, `(−1, −1)`, `(0, −2)`. The NE, SE, SW and NW neighbors share an edge
  of the diamond. The E, S, W and N neighbors share a corner.
- Tile `(x, y)` draws at `(x · ½w, y · ½h)`, row by row.

### What we do today

- A `Tile`'s `(x, y)` is a square-grid position. `WorldMap` holds `width × height` of them and
  wraps `x`.
- `MapViewport` draws `(x, y)` at `((x − y) · ½w, (x + y) · ½h)`. A grid row runs down-right
  on screen and a column runs down-left, so the poles (`y` 0 and `height − 1`) and the wrap
  seam are diagonals.
- `CameraInputController` pans and clamps in grid steps, so scrolling up or down slides along
  a diagonal.
- World generation reads latitude from `y` (landmass edge falloff, tropical moisture), so
  climate bands run diagonally too.
- The minimap draws the square grid top-down, so it does not match the main view.

## Design

### Coordinates

- `Tile::GetX()` / `GetY()` are SMAC's coordinates.
- `WorldMap(width, height)`:
  - `width` is SMAC's `axis_x`: the x wrap period, two units per tile in a row. It must be
    even, or the constructor throws.
  - `height` is the row count.
  - The map holds `width · height / 2` tiles.
- `GetWidth()` stays the wrap period, so callers that pass it to `DeltaX` and the distance
  functions keep working unchanged.
- `GetTile(x, y)` wraps `x` at `width` and returns null above the first row or below the last.
- `GetTiles()` stays row-major, north row first. The index `y · width / 2 + wrapped x / 2`
  lives in one MapUtils function, `TileIndex`, which throws when `x + y` is odd. `WorldMap`,
  `TerritoryMap` and `TileFlagMap` all index through it, so `GetTile` and every `(x, y)`
  accessor throw on odd parity. `Reset(width, height)` on `TerritoryMap` and `TileFlagMap`
  sizes them to `width · height / 2`.
- Whole-map loops iterate `GetTiles()` instead of `x`/`y` ranges: `WorldMap`'s constructor,
  `WorldGenerator`, `FactionVisibleMap` and `MinimapDisplay`. Random tile picks
  (`TerraformSpread`) draw a tile index. Tile counts come from `GetTiles().size()`
  (`Pathfinder`, `TerraformSpread`).
- `pop_composition.json`'s `bureaucracy_limit_formula` reads `map_width * map_height` as the
  tile count. Its `math.sqrt(map_width * map_height)` becomes
  `math.sqrt(map_width * map_height / 2)`.

### Lattice and neighbors

- The square lattice remains the game's geometry. A lattice step `(p, q)`, with `p` toward the
  screen's SE and `q` toward its SW, moves `(p − q, p + q)` in map coordinates. Each lattice
  offset keeps the screen direction it has today, so existing offset tables stay as they are.
  - `LatticeDelta_t {p, q}` and `LatticeDelta(rFrom, rTo, width)` take `dx` from `DeltaX` and
    `dy` from the row difference, then `p = (dx + dy) / 2`, `q = (dy − dx) / 2`.
  - `GetTileAtLatticeOffset(rMap, rOrigin, p, q)` fetches the tile one lattice offset away, or
    null off the map.
  - `ChebyshevDistance` is `max(|p|, |q|)`, which is `(|dx| + |dy|) / 2` (SMAC's
    `map_range`). `TabletopDiagonalDistance` and `InEuclideanRadius` take the lattice delta.
  - `ForEachTileInChebyshevRadius`, `ForEachTileInEuclideanRadius`,
    `ForEachTileInWorkableArea` and `ForEachOrthogonalNeighbor` iterate lattice offsets
    through `GetTileAtLatticeOffset`. Vision, auras, ZOC, territory and the 21-tile workable
    area keep their shapes, and orthogonal neighbors are the tiles across the diamond's edges.
- Every neighbor table keeps its lattice offsets and fetches through `GetTileAtLatticeOffset`:
  - `TileRenderer`'s link networks (`k_LinkDeltas`);
  - the corner neighbors in `TileRelief`, `CoastOverlay` and `WaterShading`;
  - `TileAutotile`, `TileSpriteEdgeInset`, `WorldDisplay`'s grid edges and
    `GetRiverConnections`.
- Gameplay code that steps by lattice offsets uses the same helpers:
  - `Engine`'s starting units;
  - `EvacuateTerritoryRules`' ring search, whose largest radius becomes
    `(width / 2 + height − 1) / 2`;
  - `LandmarkGeneration`'s stamps and spacing, and `TerritoryMap`'s claim distance;
  - `BaseWorkableAreaDisplay`'s layout.
- Code that wants the neighbors along a screen row or column uses map coordinates directly:
  - orographic moisture's west and east neighbors are `(x − 2, y)` and `(x + 2, y)`;
  - `Engine`'s starting forest goes on the base's corner neighbors, `(x ± 2, y)` and
    `(x, y ± 2)`.
- `Engine`'s preferred start positions step along the centre row from its first tile, every
  10 units.

### World generation

- The landmass mask, tropical moisture and pole rows keep reading `y`, which is now the screen
  row.
- Continent noise samples each tile at its position in lattice lengths, so continents keep
  their size in tiles. One unit of `x` or `y` is `√½` lattice lengths: `sampleY` is
  `y · √½ · scale`, and the cylinder's circumference is `width · √½ · scale`.
- `MapGenerationConfig_t` defaults to `width` 200, `height` 300. That is 30 000 tiles as
  today, and 4 : 3 on screen like the square-tile map. The tracked `user_settings.json`
  overrides the defaults, so its `map_generation` moves to 200 × 300 too.
- `GameSettings`' `ValidateMapGeneration_` rejects an odd `map_generation.width`, naming the
  file and key like its other checks.

### Viewport and input

- `MapViewport` puts tile `(x, y)`'s footprint box at `((x − camX) · ½w, (y − camY) · ½h)`
  from the layout's corner. It uses the wrap instance of `x` nearest the camera.
- `ForEachVisibleTile` walks rows top to bottom. It starts one row above the camera and ends
  past the layout's bottom edge by the rows a raised tile can climb. In each row it takes the
  tiles across the layout's width. Rows come out in order, which is back to front.
- `WorldCoordsAtPixel` converts the pixel to map units `(u, v)`. The flat tile under it is the
  one whose centre `(x + 1, y + 1)` satisfies `|u − x − 1| + |v − y − 1| ≤ 1`. It then checks
  the rows in front of that tile for raised tiles, front first, as today.
- `VisibleCols()` and `VisibleRows()` stay ints: the layout's size in whole map units,
  `layout.width / ½w` and `layout.height / ½h`, truncated.
- `CameraInputController`:
  - a pan step moves the camera one tile along the screen axis, 2 units in `x` or `y`, and
    `ScreenPanToCameraDelta_` goes;
  - the y clamp keeps the top and bottom rows on screen, so vertical scrolling stops at the
    poles;
  - `CenterOnTile` centres the tile using `VisibleCols()` and `VisibleRows()`.
- `TileHitTester` goes. Neither `HitTestWorldGrid` nor `HitTestBaseWorkableArea` has a caller;
  `BaseWorkableAreaDisplay` hit-tests its own tile rects.

### Minimap

- The minimap draws the map the way the main view does:
  - It is a `width × height` image in which tile `(x, y)` fills pixels `x` and `x + 1`
    (wrapped) of row `y`.
  - The image is scaled so a pixel is twice as wide as it is tall.
- A click picks the tile covering the pixel.
- The camera frame is the viewport's rectangle in map units, split at the seam as today.

## Changes

1. `MapUtils`: `TileIndex` with its parity throw, `LatticeDelta_t`, `LatticeDelta` and
   `GetTileAtLatticeOffset`. The distance, radius and orthogonal-neighbor functions move onto
   lattice offsets.
2. `WorldMap`: SMAC coordinates, the even-width check and the new storage. `TerritoryMap` and
   `TileFlagMap` (and so `FactionVisibleMap` and `FactionExploredMap`) index through
   `TileIndex`.
3. Gameplay call sites:
   - `Pathfinder`, `TerraformSpread`, `Engine`, `EvacuateTerritoryRules`,
     `LandmarkGeneration`, `TerritoryMap`, `RiverGeneration`;
   - `pop_composition.json`'s `bureaucracy_limit_formula`.
4. `WorldGenerator`: tile iteration, noise sampling and orography. `MapGenerationConfig_t`
   and `user_settings.json`: the new size. `GameSettings`: the even-width check.
5. Renderer: `GetTileAtLatticeOffset` in `TileRenderer`, `TileRelief`, `CoastOverlay`,
   `TileAutotile`, `WaterShading`, `TileSpriteEdgeInset` and `WorldDisplay`.
6. `MapViewport` and `CameraInputController`. Delete `TileHitTester`'s header and source, its
   `src/CMakeLists.txt` entry and `Engine.cpp`'s include of it.
7. `MinimapDisplay`. `BaseWorkableAreaDisplay` lays out its tiles from `LatticeDelta`.
8. Tests:
   - every test map takes an even width and addresses tiles in SMAC coordinates; tests reach
     neighbors through lattice offsets;
   - `WorldMap`: tile count, index order, wrap, null rows, and throws for odd parity and odd
     width;
   - `MapUtils`: `TileIndex` throws for odd parity, each unit lattice offset lands on the
     neighbor in its screen direction, `LatticeDelta` across the seam, Chebyshev and tabletop
     distances, and radius and workable-area tile counts (the same requirements as today, in
     the new coordinates);
   - `MapViewport`: a row's tiles share a screen y, row 0 is the top of the drawn map, the
     wrap seam is vertical, and hit tests round-trip, including raised tiles and the seam;
   - camera: pans move along the screen axes, and the vertical clamp stops at the poles;
   - minimap: the brick layout, a click resolving to a tile, and the frame split at the seam;
   - `GameSettings`: an odd `map_generation.width` is rejected.
9. Docs:
   - `map-system.md`: coordinates and the lattice helpers;
   - `graphics-system.md` and `ui-system.md`: the projection and the minimap;
     `graphics-system.md` also drops `TileHitTester`;
   - `smac-fungus-generation.md`: sample the fractal at the tile's own `(x, y)`; the pole rows
     are `y` 0 and `height − 1`.

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`, then screenshot the world map:
  - the poles run along the top and bottom rows;
  - scrolling east crosses the seam without a diagonal jump;
  - the minimap matches the main view.
