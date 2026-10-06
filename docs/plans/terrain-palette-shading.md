# Terrain palette shading: SMAC's index offsets

Draw terrain art the way SMAC does: each texel is a `palette.pcx` index, and a shade (slope
light, altitude, fog, water depth) moves it along the palette's ramps. SMAC's behavior is in
[smac-palette-lighting.md](../thinker/smac-palette-lighting.md) ("Shading is an index offset").

## Context

### What SMAC does

- Terrain art is 8-bit palette indices. Shading adds a signed offset to each texel's index.
  The palette is laid out as 16-colour ramps from light to dark, so +1 is one step darker
  along the same ramp. Every pixel is an exact palette entry; 99.5% of a SMAC screenshot's
  pixels are `palette.pcx` colours.
- The ramps shift hue and saturation along their length:
  - Arid maroon, entry 47 `(87, 46, 43)`, lightens to warm red-brown, entry 43
    `(122, 65, 51)`, and darkens to plum, entry 51 `(53, 26, 35)`.
  - Fungus darkens toward magenta.
  - Water runs from cyan (148) to navy (184).
- Vertex shades are interpolated across each triangle, and each pixel takes a whole step.
- Land shades run from −4 to +4. Fogged land gets +2. Water gets 0–37 by depth, and the deep
  art is drawn at the shade minus 16.

### What we do today

Terrain art is RGBA, and shading multiplies each vertex colour:

- land by `light_step_ratio ^ shade`;
- water by a tint table per landform (the art's mean colour moved down the ramp, divided by
  its own mean);
- fogged land by `fog_terrain_dim_ratio`.

A multiply keeps each texel's hue and saturation. Lit maroon turns salmon-pink and shaded
maroon turns brown, where SMAC shows red-brown and plum. Light above 1 needs a second,
additive pass.

## Design

- **Index art.** `extract_terrain.py` writes every `texture.pcx` terrain sprite as a
  palette-index PNG: grey is the `palette.pcx` index and alpha is coverage. This covers
  landforms, blend sets, rivers, forests, fungus, jungle, ocean, shelf, water, and coast
  water and shore.
  - `assets/sprites/palette.png` (256 × 1) holds `palette.pcx`.
  - `ter1.pcx` objects (bonuses, monolith, improvements, pods) stay RGBA and unshaded, as in
    SMAC.
  - Contact sheets stay in colour.
- **Vertices carry a shade.** `TileVertex_t` is `{x, y, shade}`. A shade is a signed number
  of palette steps, positive darker. Tint and light leave the vertex.
- **Drawing.** `DrawTileSprite(textureId, paletteId, shape)` draws the tile's four triangles
  through a fragment shader:
  - index = the texel's index + the interpolated shade rounded to the nearest step, clamped
    to 0–235 (the art range SMAC loads);
  - colour = that palette entry, with the texel's alpha.

  The shade travels in the vertex colour. `SFMLGraphics` compiles the shader when it is
  constructed and throws if the system has no shader support.
- **Land shade** is the relief's slope shade minus its altitude steps, clamped to SMAC's −4…4.
  Fogged land uses `fog_land_shade` (2) instead. Flat mode gives 0. `full_shade_rise_meters`
  and `altitude_light_steps` keep their meaning, in palette steps.
- **Water shade.** Vertex shades come from depth as now (`depth_shades`).
  - `water_shading.shades` holds `{offset, max}` per landform, applied as
    `clamp(shade + offset, 0, max)`: Ocean `{−16, 18}` and OceanShelf `{0, 15}`. The deep art
    uses indices 162–170 and the shelf art 154–156, so both stay on the water ramp, which
    ends at 188 (189 is brown).
  - Coast water uses the entry named by `coast_shades` (OceanShelf).
  - A water landform without an entry draws at shade 0.
- **Coast shore** draws at shade 0, or `fog_land_shade` when fogged.
- **Procedural fallbacks** (base fills, inset cues, river lines) keep their colours and
  `fog_fill_dim_ratio`.

## Changes

### 1. `extract_terrain.py`

- Bake every `texture.pcx` cell, blend cell and coast sprite in index space, and save it as
  an `LA` PNG (index, alpha 255 or 0) at its current path. The indices come from the sheet,
  not from colours: some entries share a colour (167 and 168).
- Write `assets/sprites/palette.png` from `palette.pcx`.
- Contact sheets resolve indices through the palette, so they look as they do now.
- Re-extract with `python3 -B extract_terrain.py --contact-sheet`.

### 2. Style (`tile_renderer`)

- Add `palette_path` (`"assets/sprites/palette.png"`) and `fog_land_shade` (2).
- In `relief`, remove `light_step_ratio`.
- In `water_shading`, replace `tints` and `coast_tints` with `shades` and `coast_shades`.
- Remove `fog_terrain_dim_ratio`.
- The parser requires the new keys. Each `shades` entry needs `0 ≤ max`, `coast_shades` must
  name an entry, and `fog_land_shade` must not be negative.
- Update `config/ui/style.json` and `tests/fixtures/ui/style.json`.

### 3. `Graphics`

```cpp
struct TileVertex_t { float x = 0.0f; float y = 0.0f; float shade = 0.0f; };
virtual bool DrawTileSprite(const std::string& textureId, const std::string& paletteId,
                            const TileShape_t& rShape) = 0;
```

- `SFMLGraphics` keeps one `sf::Shader`. Each vertex's red channel carries
  `(shade + 16) × 4`, a quarter-step precision over −16…47.75. The palette texture is a
  uniform. The additive light pass and `ScaledColor_` go.
- `NullGraphics` returns true.
- `RecordingGraphics` records the palette id with the shape.

### 4. `TileRelief`

- `TileLights_t` becomes `TileShades_t` (palette steps; 0 is as painted).
  `ResolveTileLights` becomes `ResolveTileShades`, which applies the clamp.

### 5. `WaterShading`

- `ApplyWaterShadeTint` becomes `ApplyWaterShades(TileShape_t&, const DiamondShades_t&,
  const WaterShadeRange_t&)`, which writes `clamp(shade + offset, 0, max)` into each vertex.

### 6. `TileRenderer`

- `Painted_` becomes `TerrainShape_(shape, tile, bFogged)` and `UniformShade_(shape, shade)`.
  Fogged land takes `fog_land_shade`, other land keeps the relief's shades, and water starts at
  0 before its depth shades.
- Every `DrawTileSprite` call passes the palette path, loaded like any other sprite.
- Rockiness insets interpolate shades through the shape, as they did tints.

### 7. `MapViewport`

- `ShapeAt_` writes shades. `ForEachVisibleTile(fn, bShaded)` replaces `bLit`.

### 8. Tests

- `TileReliefTests`: expectations become palette steps, for example the stepped centre in
  "Stepped lighting follows SMAC's direction-only shades" is −1.5 instead of `0.5^−1.5`.
  New: the land shade clamps to −4…4.
- `WaterShadingTests`: deep and shelf shades apply each range's offset and cap; coast
  water uses `coast_shades`.
- `TileRendererTests`:
  - every `DrawTileSprite` carries the palette path;
  - fogged land draws at `fog_land_shade` and lit land at the relief's shades;
  - water and coast draw their depth shades, and the shore draws at 0.
- `ConfigStrictnessTests`: a `shades` entry with a negative `max`, a `coast_shades` that names
  no entry, and a negative `fog_land_shade` are rejected.

### 9. Docs

- `docs/architecture/graphics-system.md`: index art, the palette shader, shades on
  `TileVertex_t`, and the new `DrawTileSprite`.
- `docs/architecture/map-system.md`: "Elevation is not a layer" describes palette shading
  instead of a colour multiply.

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`, then screenshot the map in Smooth and Stepped modes.
  - Nearly every pixel of the terrain area should be an exact `palette.pcx` colour, as in
    SMAC.
  - Next to the running SMAC window, lit slopes should go warm red-brown and shaded slopes
    plum, deep water navy, and shelf water cyan.
