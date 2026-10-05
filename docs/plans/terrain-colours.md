# Terrain colours: SMAC's palette shading

Make the world map show SMAC's colours: land art as painted, water shaded by depth, and SMAC's
fog. How SMAC does it is in
[smac-palette-lighting.md](../thinker/smac-palette-lighting.md).

## Context

### What SMAC does

- The map shows `palette.pcx` colours unchanged. Shading adds a small offset to each texel's
  palette index, sliding it along a light-to-dark ramp of the same hue.
- **Land:** slope lighting only; flat ground gets offset 0, so the sheet colours show as
  painted.
- **Water:** every vertex gets a depth shade from 0 to 37 along the blue ramp (table at
  `0x6861F0`). The tile centre uses the tile's own depth and each corner the average of the
  four tiles that share it. Shades of 16 and up draw the deep texture at `shade − 16`
  instead of the shelf texture. Shades are interpolated across the tile.
- **Fog of war:** land vertices get offset +2, about ×0.8 on the land textures. Every other
  scanline of each terrain layer is painted black. Object sprites
  (mines, bonuses, pods, the monolith) are drawn without fog.

### What we do today

- `SpriteTint_` multiplies every sprite by 60% white blended with 40% of the procedural
  elevation fill. Land comes out about 30% darker with a warm cast (measured: red ×0.65,
  blue ×0.58).
- Water gets one tint per tile from the same blend, and water landform sprites are inset
  where the shelf meets the ocean, so the fill shows in between.
- Fog dims everything to 55%, objects included.

## Design

- **Land art keeps its sheet colours.** Lit land tiles draw every sprite untinted. Procedural
  fills and fallback cues keep the elevation gradient.
- **Water is shaded per vertex by depth.** The centre and four corners each get a shade:
  - Each vertex's depth runs through `depth_shades`, which splits the range from the map's
    floor (`minElevationMeters`) up to ocean level into equal bands, deepest first.
  - The centre uses the tile's own depth.
  - A corner uses the average depth of the existing tiles among the four that share it. Land
    counts as ocean level, rows off the map are left out, and x wraps.
  - Without a map, every vertex uses the tile's own depth.

  With the shipping range (−4000 m to 0) and SMAC's 60-entry table, each band is 66⅔ m.
  Our −2000 m shelf line then lands where SMAC's shade reaches 16, the point where SMAC
  switches to the deep texture.
- **Shade becomes a colour multiply.** Each water landform's art has a tint table indexed
  by shade, taken from `palette.pcx`. Entry `s` is the mean colour of the cell's texels moved
  `s` steps down the water ramp, divided by the cell's own mean colour. Steps stop at the
  ramp's darkest entry, index 188. The deep cell's table stays white below shade 16, where
  SMAC would draw it lighter than it is painted. Shades past a table's end use its last
  entry.
- **Diamond draw.** Water art is drawn at the full tile rect as a diamond whose five vertex
  tints are interpolated across four triangles around the centre. Water landforms no longer
  inset their edges.
  - Every terrain layer and the coast draw the same way, with a uniform tint. The tile
    geometry then partitions the map exactly, and each edge pixel samples one texel inside
    the baked diamond. No fill shows between tiles.
  - Object sprites (the Improvement layer and the improvement and feature passes) keep their
    rect draw with overhang.
- **Rivers on land only.** SMAC skips river art on ocean tiles, so a river's last tile, which
  is water, gets no River layer.
- **Coast water** uses the land tile's own five vertex shades with the `OceanShelf` table
  (the coast art is baked from the shelf cell). So it meets the neighbouring water's shading
  at their shared corners.
- **Fog:**
  - Fogged land multiplies its terrain art (moisture, rockiness, landmark, vegetation, coast
    shore, river, road) by `fog_terrain_dim_ratio`. Water art is not dimmed.
  - After the Road layer, a fogged tile gets a `fog_haze_color` diamond over the whole tile:
    black at alpha 128, the average of SMAC's alternate black scanlines.
  - The Improvement layer and the improvement and feature sprite passes draw untinted on top.
  - Procedural fills, fallback cues and the minimap keep `fog_fill_dim_ratio`.

## Changes

### 1. `Graphics`

- `include/graphics/Graphics.h`: add

  ```cpp
  // Colour multiply at a tile diamond's centre and corners.
  struct DiamondTint_t
  {
      Color_t center = Color_t::White();
      Color_t west = Color_t::White();
      Color_t north = Color_t::White();
      Color_t east = Color_t::White();
      Color_t south = Color_t::White();
  };
  ```

  and `virtual bool DrawDiamondSprite(const std::string& textureId, float x, float y,
  float destWidth, float destHeight, const DiamondTint_t& rTint) = 0;`. It draws the
  texture's inscribed diamond (corners at the midpoints of its edges) onto the diamond
  inscribed in the destination rect, with the tints interpolated from the centre to each
  corner.
- `SFMLGraphics`: a triangle fan of six vertices (centre, W, N, E, S, W) with the vertex
  colours, drawn with the texture. Texture coordinates sit one texel inside the texture's
  diamond.
- `NullGraphics`: returns true.
- `tests/RecordingGraphics.h`:
  - Diamond sprites go into `sprites`, with an `std::optional<ac::DiamondTint_t>
    diamondTint` on `SpriteDraw_t`.
  - `SpriteDraw_t` and `RectDraw_t` gain `std::size_t order`, a shared draw counter, so tests
    can order sprites against fills.

### 2. Style

- `TileRendererStyle_t`:
  - Gains `float fogTerrainDimRatio`, `Color_t fogHazeColor` and
    `WaterShadingStyle_t waterShading`.
  - Loses `spriteEdgeInsetRatio`; with the water inset gone, nothing reads it.

  ```cpp
  struct WaterShadingStyle_t
  {
      // Shade for equal depth bands from the map's floor up to ocean level, deepest first.
      std::vector<int> depthShades;
      // Colour multiply of each water landform's art per shade, keyed by landform id.
      std::unordered_map<std::string, std::vector<Color_t>> tints;
      // The tints entry coast water uses.
      std::string coastTints;
  };
  ```

- `UiStyle.cpp` (`tile_renderer`):
  - New required keys: `fog_terrain_dim_ratio` (in [0, 1]), `fog_haze_color`, and
    `water_shading` with `depth_shades`, `tints` and `coast_tints`.
  - `water_shading` must have non-empty `depth_shades` of non-negative shades, a non-empty
    `tints` object with no empty table, and a `coast_tints` that names a `tints` entry.
  - Drop `sprite_edge_inset_ratio`. Split the array-to-colour part of `ParseColor_` out so
    table entries reuse it.

### 3. `WaterShading` (new: `include/ui/WaterShading.h`, `src/ui/WaterShading.cpp`)

```cpp
struct DiamondShades_t
{
    int center = 0;
    int west = 0;
    int north = 0;
    int east = 0;
    int south = 0;
};

DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap* pMap,
                                   const std::vector<int>& depthShades);
DiamondTint_t WaterShadeTint(const DiamondShades_t& shades, const std::vector<Color_t>& tints);
```

- **Band index:** `floor(n · (e − floor) / (ocean − floor))`, capped at `n − 1`, where `e` is
  the elevation clamped to [floor, ocean], `n` is the table size, and floor and ocean come
  from the tile's map rules.
- **Corners:** the corner tiles are N (−1,−1) (0,−1) (−1,0); E (0,−1) (+1,−1) (+1,0);
  S (+1,0) (+1,+1) (0,+1); W (0,+1) (−1,+1) (−1,0), each plus the tile itself.
- **Errors:** throws on an empty `depthShades` or `tints`.
- **Build files:** added to `src/CMakeLists.txt` and `tests/CMakeLists.txt`.

### 4. `TileRenderer`

- **Tints:**
  - `TerrainTint_(rTile, bFogged)` replaces `SpriteTint_`: white, or white dimmed by
    `fog_terrain_dim_ratio` on fogged land.
  - Remove `k_ElevationTintBlend`, `AverageSpriteTint_` and the separate fungus tint.
- **Water landform:**
  - Resolve the occupant and its sprite path as for any layer.
  - Draw it with `DrawDiamondSprite` at the full tile rect, tinted with
    `WaterShadeTint(ResolveWaterShades(...), tints)` from the table named by the layer's
    content id. With no table it draws untinted.
  - A shared load-and-cache helper serves both `TryDrawSprite_` and the new diamond draw.
  - Remove the water landform inset branch.
- **Coast:**
  - Water sprites use `DrawDiamondSprite` with the land tile's shades and the
    `coast_tints` table; shore sprites use the terrain tint.
  - "No coast" is now all corner masks being 0.
- **Haze:** a fogged tile draws the `fog_haze_color` diamond once, before the Improvement
  layer.
- **Terrain diamonds:** every layer except Improvement, and the coast shore, draw with
  `DrawDiamondSprite` and a uniform tint.
- **Objects:** the Improvement layer and the improvement and feature passes draw untinted.
- **Header:** update `include/ui/TileRenderer.h`'s comments to match.

### 5. `TileLayerResolver`

`ResolveRiverLayer_` returns the river only for land tiles.

### 6. Retire the old paths

- `TileSpriteEdgeInset`: remove `MatchSeaLandformEdges`. `MatchRockinessEdges` and
  `DestRectForEdgeInsets` stay.
- `CoastOverlay_t`: drop `waterNeighbors`.

### 7. Configs

- `config/ui/style.json` (`tile_renderer`):
  - `fog_terrain_dim_ratio: 0.8` and `fog_haze_color: [0, 0, 0, 128]`.
  - `water_shading`: `depth_shades` is SMAC's table for details 0–59. `tints` holds
    `OceanShelf` and `Ocean` tables of 38 entries (shades 0–37) computed as described above.
    `coast_tints` is `"OceanShelf"`.
  - Remove `sprite_edge_inset_ratio`.
- `tests/fixtures/ui/style.json`:
  - The same keys with fixture-owned values: a 4-entry `depth_shades` of `[3, 2, 1, 0]`
    (1000 m bands on the fixture's −4000 m floor) and distinct 4-entry tables.
  - Remove `sprite_edge_inset_ratio`.

### 8. Tests

- **`tests/ui/WaterShadingTests.cpp` (new):**
  - The centre takes the band of the tile's own depth: the floor gives the first entry, just
    below ocean level gives the last.
  - A corner takes the band of the average depth of its four tiles.
  - Land counts as ocean level.
  - Rows off the map are left out of a corner's average, and corners wrap across the x seam.
  - Without a map, every vertex uses the tile's own depth.
  - Tints come from the table by shade, and a shade past the table's end uses its last entry.
- **`tests/ui/TileRendererTests.cpp`:**
  - Lit land draws its moisture sprite untinted at any elevation.
  - A water tile's art is a diamond sprite at the full tile rect, with centre and corner
    tints from its own depth and its neighbours' depths.
  - Coast water uses the land tile's vertex shades and the shelf table: shallowest at the
    centre, and the corners shared with water match the water tile's corners. This replaces
    the two "coast water takes / averages the water tiles' tint" sections.
  - Fog dims land art by `fog_terrain_dim_ratio` and leaves water art undimmed. The haze
    diamond comes after the last terrain sprite and before object sprites, and object
    sprites stay untinted. This replaces "fog dims water and shore alike".
  - Coast sprites are found by texture id through the shared `sprites` list.
  - The moisture base is a diamond draw; an object sprite is not.
- **`tests/game/MapRulesTests.cpp`:** a river's water tile has no River layer.
- **`tests/ui/TileSpriteEdgeInsetTests.cpp`:** remove "Coastal land neighbors do not inset
  sea landform sprites".
- **`tests/ui/CoastOverlayTests.cpp`:** remove the `waterNeighbors` checks.
- **`tests/game/ConfigStrictnessTests.cpp`:** style loading rejects a `coast_tints` that
  names no table and an empty `depth_shades`.

### 9. Docs

- `docs/architecture/graphics-system.md`:
  - Add `DrawDiamondSprite` to the Graphics interface diagram and method list.
  - TileRenderer bullets: land art untinted, water depth shading (`WaterShading`), coast
    water shading, fog dim and haze, and edge insets for rockiness only.
- `docs/architecture/map-system.md`: the "Elevation is not a layer" paragraph describes the
  water depth shading and untinted land art.

## Verification

- `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- `./bd build`. From the scratch run folder with a fixed seed, screenshot the map before and
  after and compare against the SMAC capture. Check that:
  - land is brighter;
  - water runs from light teal at the coast to deep blue offshore with no tile seams;
  - coast water meets the open water smoothly.
- With fog enabled in the scratch settings copy, check the haze on remembered tiles.
