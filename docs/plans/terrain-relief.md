# Terrain relief: raised tiles, slope lighting and the map grid

Show elevation the way SMAC does: tile corners rise with the terrain, slopes are lit from one
direction, and grid lines follow the raised edges. A map-display setting switches between
smooth heights, SMAC's whole-level steps, and no relief. SMAC's behavior is in
[smac-palette-lighting.md](../thinker/smac-palette-lighting.md) ("Relief", "Grid").

## Context

### What SMAC does

- **Raised vertices.** A land tile's centre rises `(level − 3)` altitude levels; one level is
  about 1000 m and lifts a quarter of a tile's width at any zoom. A corner sits at the mean
  of the four tiles that share it, except that a corner touching water or the map's top or
  bottom edge stays at sea level. Water tiles are flat. Each tile is drawn as four triangles
  around its raised centre.
- **Slope lighting.** Each facet (the centre and two adjacent corners) is shaded by the
  direction it faces: up to four palette steps lighter facing the screen's lower right, four
  darker facing the upper left, nothing when flat. A vertex averages the facets that meet
  there. Water is not lit, and tiles out of sight get a flat shade instead.
- **Grid.** Lines run along every tile edge through the raised corners. Edges between land
  tiles are dark green; edges touching water are dark blue and appear only with the
  ocean-grid preference.
- Relief display can be turned off.

### What we do today

Tiles are flat diamonds at their grid position. Nothing shows elevation, and the tile border
(`tile_border_width: -1`) draws nothing.

## Design

- **Setting.** `user_settings.json` gains a `map_display` section:
  - `relief` is `"flat"`, `"smooth"` or `"stepped"` (default `"smooth"`).
  - `ocean_grid` is a bool (default false).

  The settings panel shows both under a "Map Display" header, and changes apply on the next
  frame.
- **Heights.** A vertex's lift is measured in tile widths, so it scales with zoom.
  - A land centre lifts `elevation / level_meters × lift_per_level_ratio` in smooth mode, and
    `floor(elevation / level_meters) × lift_per_level_ratio` in stepped mode. Flat mode lifts
    nothing.
  - A corner takes the mean of its four tiles' centre lifts, or 0 when any of them is water or
    off the map. x wraps.
  - Water tiles do not lift.
  - Unexplored tiles keep their real lift, so the map has no seams at the shroud's edge.
- **Lighting** applies to land in sight, with relief on.
  - The corners are W, N, E, S, and facet `k` lies between corners `k` and `k + 1`.
  - A facet's shade is `−3·(s + t) / max(√(s² + t²), r)`. Here `(s, t)` is
    `(a_k, a_{k+1})` turned a quarter turn per facet (`(a, b)`, `(−b, a)`, `(−a, −b)`,
    `(b, −a)`), `a_c` is corner `c`'s height above the centre in quarter levels, and `r` is
    `full_shade_rise_meters` in quarter levels.
  - A slope gets full shade once a corner rises `r` above the centre, and gentler slopes shade
    in proportion. SMAC's `r` is one quarter level. Stepped heights give SMAC's whole quarter
    levels, so any `r` up to one quarter level matches SMAC exactly. Smooth heights rise mostly
    100–300 m between neighbors, so a smaller `r` (100 m) lets their slopes show.
  - A vertex shade is the mean of the facets that meet at it: four at the centre, and at a
    corner the two facets of each of the four tiles sharing it, water counting as 0.
  - Altitude lightens the vertex by `altitude_light_steps` steps per level of its height above
    sea level (its corner height at a corner), so higher ground reads brighter. SMAC has no
    such term; 0 turns it off.
  - Light is `light_step_ratio ^ shade`.
  - Water, fogged tiles and flat mode get light 1.
- **One geometry.** `MapViewport` owns relief:
  - Every visible tile comes with its raised shape (centre and four corners).
  - Tile centres and footprint origins report the raised centre.
  - A click picks the frontmost tile whose raised shape contains the point.
  - The visible range reaches far enough down to include tiles raised into view.
- **Drawing on the shape.** Fills, terrain sprites, coast overlays, the fog haze and
  procedural cues all draw on the raised shape.
  - Inset sprites (rockiness) and inset cues map their flat sub-diamond through the tile's
    four triangles.
  - Object sprites (bonuses, monolith, improvements) keep a flat footprint centred on the
    raised centre, and units and bases sit there too.
  - Land terrain layers multiply each vertex's light into their tint. Water and coast water
    keep their depth shading, and the coast shore stays unlit.
- **Grid.** Right after a tile is drawn, it draws its NW and NE edges along its raised corners,
  so every edge is drawn once and raised tiles in front cover the lines behind them.
  - An edge between two land tiles uses `grid_land_color`.
  - An edge touching water uses `grid_water_color`, and only with `ocean_grid` on.
  - Unexplored tiles draw all their edges in `grid_land_color`, so the grid does not reveal
    coastlines.

## Changes

### 1. Settings

- `include/game/MapDisplayConfig.h` (new):

  ```cpp
  enum class ReliefMode_t { Flat, Smooth, Stepped, };
  struct MapDisplayConfig_t
  {
      ReliefMode_t relief = ReliefMode_t::Smooth;
      bool bOceanGrid = false;
      bool operator==(const MapDisplayConfig_t&) const = default;
  };
  ```

- `GameSettings` gains `GetMapDisplay()`, `SetMapDisplay()` and an `OnMapDisplayChanged`
  signal.
  - `Load` reads `map_display`: `relief` through magic_enum, case-insensitive. An unknown
    value throws, naming the file and key.
  - `Save` writes the section.
- `SettingRowKind_t` gains `Choice`: a row that shows `getValueText` and calls a new
  `cycle(GameSettings&)` callback on a left click, then saves. `RequireCallbacks_` checks
  both callbacks.
- `SettingsPanel` rows:
  - A "Map Display" header.
  - "Elevation" (Choice; Smooth → Stepped → Flat).
  - "Ocean Grid" (Bool).

### 2. Style (`tile_renderer`)

- Add `relief`:
  - `lift_per_level_ratio` (0.26: SMAC's 25 px per level on a 96 px tile);
  - `level_meters` (1000);
  - `light_step_ratio` (0.9, one palette step on the terrain ramps);
  - `full_shade_rise_meters` (100);
  - `altitude_light_steps` (1.0).
- Add `grid_land_color` (`[21, 41, 24, 255]`, `palette.pcx` 27), `grid_water_color`
  (`[14, 37, 75, 255]`, `palette.pcx` 180) and `grid_line_width` (1.0).
- Remove `tile_border_color` and `tile_border_width` from `tile_renderer`. The base screen's
  own keys stay.
- The parser requires all of these: the ratios, `level_meters` and `full_shade_rise_meters`
  positive, and `altitude_light_steps` at least 0. `tests/fixtures/ui/style.json` gets fixture
  values.

### 3. `TileRelief` (new: `include/ui/TileRelief.h`, `src/ui/TileRelief.cpp`)

```cpp
struct TileLifts_t { float center = 0, west = 0, north = 0, east = 0, south = 0; };
struct TileLights_t { float center = 1, west = 1, north = 1, east = 1, south = 1; };

TileLifts_t ResolveTileLifts(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                             const ReliefStyle_t& rStyle);
TileLights_t ResolveTileLights(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                               const ReliefStyle_t& rStyle);
```

Corner neighbors are the same sets `CoastOverlay` and `WaterShading` use. Lights read the lifts
of the tiles around each corner.

### 4. `Graphics`

- Replace `DiamondTint_t` and `DrawDiamondSprite` with a shape:

  ```cpp
  struct TileVertex_t { float x = 0; float y = 0; Color_t tint = Color_t::White(); float light = 1; };
  struct TileShape_t { TileVertex_t center, west, north, east, south; };
  virtual bool DrawTileSprite(const std::string& textureId, const TileShape_t& rShape) = 0;
  virtual void FillTileShape(const TileShape_t& rShape, const Color_t& color) = 0;
  ```

  `DrawTileSprite` maps the texture's inscribed diamond (sampled one texel inside, as now)
  onto the five vertices. Each vertex's colour is `tint × light`.
- `SFMLGraphics` draws the fan once with `tint × min(light, 1)`. When any vertex has
  `light > 1`, it draws a second additive pass with `tint × (light − 1)`, so lighting can
  brighten as well as darken. `FillTileShape` is an untextured fan.
- `NullGraphics` returns true or does nothing.
- `RecordingGraphics` records the shape on `SpriteDraw_t`, and records fills as rects of the
  shape's bounding box, also carrying the shape.
- `WaterShadeTint` becomes `ApplyWaterShadeTint`, which writes its five tints into a
  `TileShape_t`'s vertices.

### 5. `MapViewport`

- `SetRelief(ReliefMode_t, const ReliefStyle_t&)`.
- `ForEachVisibleTile` calls `fn(const Tile&, const TileShape_t&)` with the raised shape. Its
  row range extends by the lift of the map's highest elevation (`maxElevationMeters`).
- `PixelOriginOf` and `PixelCenterOf` return the raised footprint origin and centre.
- `WorldCoordsAtPixel` tests the flat candidate and the tiles below it within that same
  margin, front to back, against their raised shapes (four triangles).

### 6. `TileRenderer`

- `Render(Graphics&, const Tile&, const TileShape_t&, bool bFogged, const WorldMap*)`.
  `FlatTileShape(x, y, size)` builds the flat shape for the location preview.
- Every fill and terrain draw uses the shape:
  - The base fill and haze use `FillTileShape`.
  - Terrain layers and coast use `DrawTileSprite`.
  - Rockiness and procedural insets map their sub-diamond through the shape.
- Land terrain layers carry the tile's lights; water, coast water and coast shore do not.
- Object sprites draw at the raised centre with their flat footprint and overhang.
- The tile border draw goes.

### 7. `WorldDisplay`

- Each frame, set the viewport's relief from `GameSettings::GetMapDisplay()`.
- Per tile:
  - shroud is `FillTileShape` with `shroudColor`;
  - every other tile goes through `TileRenderer::Render` with the shape and the tile's lights;
  - then the grid edges.
- Bases, sensors, monoliths, path preview and unit markers already go through
  `PixelOriginOf` / `PixelCenterOf`, so they follow the raised centre.

### 8. Tests

- **`tests/ui/TileReliefTests.cpp`:**
  - Lifts per mode: a land centre at 1500 m lifts 1.5 levels smooth, 1 stepped and 0 flat.
  - A corner is the mean of its four centres; touching water or the map edge pins it to 0.
  - Water is flat.
  - Lights:
    - flat land and water are 1;
    - a facet rising toward the upper left is lighter and the opposite darker;
    - stepped results match SMAC's integer formula, for any full-shade rise up to a quarter
      level;
    - in smooth mode a slope gentler than the full-shade rise shades less than a steeper one,
      and slopes at or above it shade the same;
    - level land lightens by the altitude light per level, and water stays 1;
    - flat mode gives 1.
- **`tests/ui/MapViewportTests.cpp`:**
  - A raised tile's centre is above its flat centre.
  - A click on a raised tile's shape picks it over the tile behind.
  - A tile raised into view from below the layout is visited.
  - Flat mode matches today's projection.
- **`tests/ui/TileRendererTests.cpp`:**
  - Sprites and fills land on the given shape.
  - Object sprites sit at the raised centre.
  - Land terrain sprites carry the lights; water and coast do not.
  - Existing sections move to `FlatTileShape`.
- **Grid** (`WorldDisplay`):
  - land–land edges use the land colour;
  - water edges appear only with `ocean_grid`;
  - unexplored edges use the land colour;
  - edges run through the raised corners.
- **`GameSettings`:** `map_display` loads, saves and round-trips, and rejects an unknown
  relief value.
- **`SettingsPanel`:** a click on the Choice row cycles Smooth → Stepped → Flat and saves.
- **Style:** loading rejects a non-positive relief ratio or full-shade rise, and a negative
  altitude light.

### 9. Docs

- `docs/architecture/graphics-system.md`:
  - `MapViewport` relief and hit-testing.
  - `TileShape_t`, `DrawTileSprite` and `FillTileShape` in the Graphics interface diagram and
    method list.
  - `TileRenderer` drawing on shapes, `TileRelief`, and the grid.
- `docs/architecture/map-system.md`: "Elevation is not a layer" describes the relief and
  lighting.
- The Map Display setting and its settings-panel rows are described with the relief in
  `graphics-system.md`.

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`. With a fixed seed, screenshot each relief mode and check that:
  - hills rise and coasts meet the waterline;
  - lighting is brighter on slopes facing the lower right;
  - grid lines bend over hills;
  - clicking a raised tile selects it;
  - units and bases sit on the raised centres.
- Turn `ocean_grid` on and check the water grid.
