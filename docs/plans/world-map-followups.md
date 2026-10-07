# World map follow-ups (roadmap Phase 1c)

Finish the world map before bases: improvement art, SMAC's seat for units and bases, and map
content closer to a SMAC map. SMAC's behavior is in
[smac-terrain-textures.md](../thinker/smac-terrain-textures.md); landmark art and landmark
lighting get their own plan once SMAC's landmark placement is reverse-engineered.

## Context

### What SMAC does

`MapWin_gen_terrain_poly` (`0x4632D0`) draws one tile.

- **Seat.** `MapWin_tile_to_pixel` (`0x462F00`) returns the tile's flat bounding-box top-left
  raised by the mean of its four corner lifts. Every caller uses it, units and bases included.
  Objects draw from that point (the tile's top corner).
- **Farm ground** replaces the land base cell: `texture.pcx` (775, 219), 3 × 3 cells of 56 px
  on a 57 px grid. Column = rainfall (arid, moist, wet), row = a per-tile random 0–2.
- **Roads and mag tubes** (`0x465B41` loop), land tiles only. A tile carries a road if it has
  one or is a base; it carries a tube if it has one or is a base. For each of the eight
  neighbors that is land and carries a road:
  - it draws one cell, `1 + ((dir + 2) & 7)`;
  - the cell is the tube cell when both tiles carry tubes, otherwise the road cell.

  Cells run hub, NW edge, N corner, NE edge, E corner, SE edge, S corner, SW edge, W corner
  (road set (775, 395), tube set (775, 566), mapped like forest and river cells). A tile that
  is not a base draws the road hub when it drew no link, and the tube hub when it carries a tube
  but drew no tube link.
- **Improvement sprites** (`ter1.pcx`, 100 × 62, drawn at the seat):

  | Improvement | Land | Sea |
  |---|---|---|
  | Farm structures | (923, 453 + 63·row), row = clamp(nutrients − 1, 0, 3) | Kelp (607, 190) |
  | Soil enricher | (822, 453 + 63·row), drawn instead of the farm structures | — |
  | Mine | (607, 64) | Mining platform (506, 64) |
  | Solar collector | (607, 127) | Tidal harness (506, 127) |
  | Condenser, echelon mirror, thermal borehole | (506, 253), (607, 253), (708, 253) | — |
  | Bunker, airbase | (506, 316), (607, 316) | — |
  | Sensor | (708, 316) | (708, 316) |

  Order: kelp, roads and tubes, tile bonuses, bunker, airbase, sensor, farm structures, mine,
  solar, condenser, mirror, borehole, Monolith.

### What we do today

- Units, base labels and markers sit on the raised centre.
- Sensors and Monoliths draw as procedural markers on top of the map.
- Roads, tubes, farms and every improvement but the forest have no art; `improvements.json`
  has no sprite paths for them.
- Fungus covers 8% of tiles and aquifers 0.2%; SMAC maps show more fungus and rivers.

## Design

- **Seat.** `MapViewport::PixelOriginOf`, `PixelCenterOf` and `FootprintOrigin` use the mean
  of the four corner lifts, as terrain objects already do.
- **Extraction.** `extract_terrain.py` adds:
  - index-art tile sets `landforms/road/{0..8}.png`, `landforms/mag_tube/{0..8}.png` and
    `landforms/farm/{arid,moist,wet}_{0..2}.png`;
  - RGBA object sprites under `sprites/improvements/`: `farm_{0..3}`, `soil_enricher_{0..3}`,
    `kelp_farm`, `mine`, `mining_platform`, `solar_collector`, `tidal_harness`, `condenser`,
    `echelon_mirror`, `thermal_borehole`, `bunker`, `airbase` and `sensor`, with the same key
    and shadow handling as the tile bonuses.
- **Config** (`improvements.json`, new optional fields on `ImprovementConfig_t`):
  - Object sprites use the existing `sprite_paths.land` / `.sea` with
    `sprite_overhang_ratio` 0.24.
  - `sprite_yield_rows: {"stat": "nutrients", "land": [4 paths]}` picks the row
    `clamp(yield − 1, 0, rows − 1)` (Farm, SoilEnricher).
  - `hides_sprites_of: ["Farm"]` (SoilEnricher): while it is present the listed occupants'
    object sprites are not drawn.
  - `ground_sprites: {"Arid": [...], "Moist": [...], "Wet": [...]}` (Farm): drawn in place of
    the tile's moisture base, one variant per tile.
  - `sprite_tiles` layout `links` (Road, MagTube) with `link_occupants` (the occupant and
    `Base`), and `replaces_links_of: "Road"` on MagTube.
- **Drawing.**
  - The Moisture layer draws the ground sprite of any occupant with one for the tile's
    moisture.
  - The Road layer draws the link networks per SMAC's rule above.
  - `RenderObjects` draws tile bonuses, then every improvement with object art in
    `improvements.json` order (skipping hidden ones), then the other terrain objects
    (Monolith).
  - Yield rows come from a yield lookup the caller passes: `WorldDisplay` resolves it through
    `GameState::GetTileEffects()`. The location preview passes none and draws the first row.
  - Object art that is configured but fails to load draws a magenta and black 2 × 2 checker
    at the seat (`tile_renderer.missing_art_color`, `missing_art_alt_color`,
    `missing_art_size_ratio`). `WorldDisplay`'s sensor and Monolith markers go: both draw
    their art like every other object.
- **Map content.** `decoration.json` takes SMAC's world-builder amounts at average settings
  (`alphax.txt` `#WORLDBUILDER`, placement as in Thinker's `mapgen.cpp`):
  - `fungus.fraction` 0.29: SMAC marks tiles whose noise falls in three bands, widened by the
    native-life setting, which covers 14%, 29% and 44% of tiles for rare, average and
    abundant life.
  - `aquifers.fraction` 0.02: SMAC seeds `tiles · (4 − ocean) · (16 + 16 · cloud) / 9600`
    river sources, 1% of tiles or about 2% of land at average ocean and clouds. On a 200 × 150
    pangea that gives rivers on 12% of land.

## Changes

1. `MapViewport`: corner-mean seat for `PixelOriginOf`, `PixelCenterOf`, `FootprintOrigin`.
2. `extract_terrain.py`: the tile sets and object sprites above; contact sheets include them.
3. `ImprovementConfig_t` + parser: `sprite_yield_rows`, `hides_sprites_of`, `ground_sprites`,
   `link_occupants`, `replaces_links_of`, layout `links`; unknown ids in the new lists are
   rejected.
4. `config/improvements.json` and `tests/fixtures/improvements.json`: art for every improvement
   above.
5. `TileRenderer`: ground sprites, link networks, object pass order, yield rows, hidden
   sprites; `RenderObjects` / `Render` take the nutrient yield.
6. `WorldDisplay`: pass the yield lookup; drop the sensor and Monolith markers and their
   style keys.
7. `decoration.json`: SMAC's fungus and river-source fractions.
8. Tests:
   - seat at the corner mean;
   - link cells for each direction, tube over road, hubs, base tiles;
   - farm ground per moisture and variant;
   - yield rows and clamping;
   - hidden farm sprites under a soil enricher;
   - sea and land variants;
   - the missing-art checker;
   - parser rejections.
9. Docs: `graphics-system.md` (TileRenderer improvements, seat), `smac-terrain-textures.md`
   (road and farm rules, improvement draw order).

## Verification

- Run `./bd test`, then check `build/Testing/Temporary/LastTest.log`.
- Run `./bd build`, then screenshot a base with roads, farms and mines next to the SMAC window.
