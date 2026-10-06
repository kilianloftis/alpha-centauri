# Sea depth shading: SMAC's depth scale and deep-water art

Shade and texture water the way SMAC does, so the sea darkens from cyan shelf to navy deep water
with depth, and draw the ocean shelf where SMAC does. SMAC's behavior is in
[smac-palette-lighting.md](../thinker/smac-palette-lighting.md) ("Water depth").

## Context

### What SMAC does

- A water vertex's depth detail is `60 + metres / 50`: 60 is sea level, and each detail step
  is 50 m. Detail 0 (3000 m down) and anything deeper share the deepest shade.
- The detail picks a shade from the table at `0x6861F0` (0–37; 16 is about 1450 m down).
- Altitude level 2 (details 40–59, the top 1000 m of water) is the ocean shelf.
- A tile draws the shelf art when every corner's shade is below 16. Otherwise it draws the
  deep art at the shade minus 16. The choice is per tile and follows depth, not the terrain
  type.

### What we do today

- `depth_shades` splits the whole range from the map's floor (−4000 m) up to sea level into
  60 equal bands of 66⅔ m. The water reads shallower than in SMAC.
- `ocean_shelf_meters` is −2000, twice SMAC's depth.
- The art follows the landform: OceanShelf (above −2000 m, 88% of our sea) always draws the
  shelf art. So almost all of our sea is cyan-teal, and only the deepest 12% is navy.

## Design

- **Shelf rule.** `ocean_shelf_meters` becomes −1000 in `config/map_rules.json`, SMAC's shelf
  line. Everything that reads it (the OceanShelf landform and its +1 nutrients, terraform spread
  at sea) follows.
- **Depth scale.** `water_shading.detail_meters` (50) sets the depth of one detail step. A
  vertex's detail is `floor(N + (elevation − ocean level) / detail_meters)`, clamped to
  0…N − 1, where N is the size of `depth_shades` (SMAC's 60-entry table). Corners average
  their tiles' elevations as now. The map's floor no longer enters the scale.
- **Deep or shelf art per tile.** `water_shading.deep_landform` (Ocean),
  `shelf_landform` (OceanShelf) and `deep_from_shade` (16) apply on a tile whose landform is
  either of them:
  - if any corner's depth shade reaches `deep_from_shade`, the tile draws the deep landform's
    art within its shade range;
  - otherwise it draws the shelf landform's art within its range.

  Other water landforms keep their own art and range.
- Coast water keeps the shelf range (`coast_shades`), as SMAC draws the coast from the shelf
  texture.
- As in SMAC, the art and the shelf rule do not split at the same depth: ocean between −1000 m
  and about −1450 m draws the shelf art.

## Changes

### 1. Map rules

- `config/map_rules.json`: `ocean_shelf_meters` −1000. Test fixtures keep their own value.

### 2. Style (`tile_renderer.water_shading`)

- Add `detail_meters` (50), `deep_landform`, `shelf_landform` and `deep_from_shade` (16).
- The parser requires them:
  - `detail_meters` must be positive;
  - both landform names must have a `shades` entry;
  - `deep_from_shade` must not be negative.
- `depth_shades` stays SMAC's table.
- Update `config/ui/style.json` and `tests/fixtures/ui/style.json`.

### 3. `WaterShading`

- `ResolveWaterShades(rTile, pMap, rShading)` takes the water shading style and uses the detail
  scale. Its comment and header describe SMAC's scale instead of floor-to-sea bands.
- New: `const std::string& SeaArtLandform(const DiamondShades_t&, const WaterShadingStyle_t&)`
  returns the deep or shelf landform id per the rule above (corners only).

### 4. `TileRenderer`

- `TryDrawWaterLandform_`: for the deep and shelf landforms, choose the art and range from the
  tile's shades. The deep art's path comes from that landform's `sprite_paths`, picked like any
  other occupant's.

### 5. Tests

- `WaterShadingTests`:
  - details follow 50 m steps from sea level;
  - water deeper than N steps takes the deepest shade;
  - the art turns deep when any corner reaches `deep_from_shade` and stays shelf otherwise;
  - the centre alone does not switch it.
- `TileRendererTests`:
  - a shelf-rule tile with a corner past the line draws the deep art;
  - an Ocean-rule tile with every corner shallow draws the shelf art;
  - coast water keeps the shelf range.
- `ConfigStrictnessTests`: a non-positive `detail_meters`, and a `deep_landform` with no
  `shades` entry, are rejected.

### 6. Docs

- `docs/architecture/graphics-system.md` ("Water shading"): the detail scale and per-tile art.

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`, then screenshot the open sea beside the SMAC window. Navy should cover
  water deeper than about 1450 m, with cyan shelf toward the coasts.
