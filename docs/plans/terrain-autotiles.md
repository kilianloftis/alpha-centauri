# Terrain autotiles: SMAC's neighbor-driven terrain art

Follows step 1 (per-surface `sprite_paths`, real forest / fungus / river / bonus crops). Sheet
layouts and SMAC's selection code are in
[smac-terrain-textures.md](../thinker/smac-terrain-textures.md).

## Context

### What SMAC does

- **One base texture per land tile.** Arid tiles draw the arid cell. Moist and wet tiles draw
  one cell from a 16-cell set, chosen by which neighbors are ocean or at least as wet. The
  cell fades out toward the other neighbors. There is no tier stacking.
- **Edge sets (forest, river).** 16 cells, indexed by a 4-bit mask: which of the four
  edge-sharing neighbors have the same feature. No rotation.
- **Blend sets (moist, wet, land fungus, sea fungus, jungle).** An 8-neighbor mask selects one
  of 15 shapes and a quarter-turn rotation from a 256-entry table (`0x685484`). The table is
  the standard 47-blob reduction: a corner neighbor counts only when both edge neighbors next
  to it are set. That holds for 254 of the 256 entries. Masks 202 and 218 select a four-edge
  shape for a three-edge mask (data errors in SMAC); reducing the mask fixes them.
- **Draw order:** base, rocky overlay, landmark (jungle), forest, fungus, coast, river,
  improvement sprites.

### Orientation

Verified against the cell art: blend cell 1 reaches only the top side, cell 2 top + right,
cell 9 all four sides; forest/river cell 1 reaches the right side, 2 the bottom, 4 the left,
8 the top. Screen corners W, N, E, S take these texture corners:

| Sets | W | N | E | S |
|------|---|---|---|---|
| Blend sets and plain cells (arid, rolling, rocky, water, shelf, ocean), rotation 0 | BL | TL | TR | BR |
| Edge sets (forest, river) | TL | TR | BR | BL |

Rotation `r` shifts the blend row: corner `k` (W = 0 … S = 3) takes the rotation-0 corner
`(k + r) & 3`.

### Masks on our grid

- **Edge mask:** bit 0 (0, −1), bit 1 (+1, 0), bit 2 (0, +1), bit 3 (−1, 0) — the diamond's
  NE, SE, SW, NW edges. Same order as `RiverConnection_t`, so a river's mask is
  `GetRiverConnections`.
- **Blob mask:** clockwise from the N corner — bit 0 (−1, −1), 1 (0, −1), 2 (+1, −1),
  3 (+1, 0), 4 (+1, +1), 5 (0, +1), 6 (−1, +1), 7 (−1, 0). Corner bits 0, 2, 4, 6 stay only
  when both neighboring edge bits are set; 47 masks remain.
- **What sets a bit:** in the Moisture layer, a neighbor that is water or at least as wet
  as the tile. Everywhere else, a neighbor with the same occupant (`Tile::HasFeature(id)`, which
  covers rivers, terrain features and improvements). Rows off the map never set a bit; x
  wraps.

Blend shapes per reduced mask (`mask:shape` + rotation `a`–`d` = 0–3):

```text
0:0a   2:1a   8:1d  10:2a  14:3a  32:1c  34:4a  40:2d  42:5a  46:6a  56:3d  58:7a
62:8a 128:1b 130:2b 131:3b 136:4d 138:5b 139:6b 142:7b 143:8b 160:2c 162:5c 163:7c
168:5d 170:9a 171:10a 174:10d 175:11a 184:6d 186:10c 187:12a 190:11d 191:13a 224:3c
226:6c 227:8c 232:7d 234:10b 235:11b 238:12d 239:13b 248:8d 250:11c 251:13c 254:13d
255:14a
```

## Design

- **Bake every texture cell into the diamond.** `extract_terrain.py` maps each 56×56 cell
  onto a 112×56 diamond in SMAC's orientation, with blend rotations baked in, since the
  renderer cannot rotate. Coverage is the half-open test `0 ≤ s, t < 1`, so neighboring tiles
  partition the plane: no gaps, no overlaps.
- **One sprite per mask.** Tile sets live in `sprites/landforms/<set>/<mask>.png`: 47 files
  per blend set, 16 per edge set.
- **Config names the set.** A new `sprite_tiles` entry gives the layout and a per-surface
  path pattern containing `{mask}`. The renderer computes the mask and formats the path. It
  needs no SMAC table.

## Changes

### 1. `extract_terrain.py`

- **Diamond bake.** Replace the squash-and-mask path (`apply_diamond_mask`) with a bake that
  samples the cell at `W + s·(N − W) + t·(S − W)` in texture space (corners from the table
  above, in 56-unit coordinates; `s`, `t` as in the coast bake) and keys indices 0 and 255.
  Apply it to arid, rolling, rocky, water, ocean shelf and ocean. Remove `--no-diamond-mask`.
- **Blend sets.** Add the 47-entry table above as a constant (`terranx.exe` `0x685484`, read
  at the reduced masks). Write `moist/`, `wet/`, `fungus_land/`, `fungus_sea/` and `jungle/`:
  for each reduced mask, bake cell `shape` with rotation `r`. Jungle cells come from
  (526, 259), 15 cells.
- **Edge sets.** Write `forest/` and `river/`: mask `m` is cell `m`, fixed orientation.
- **Stop writing the raw cells** (`moist_0.png` … `river_15.png`).
- **Contact sheet.** With `--contact-sheet`, also write `sprites/_tiles_contact_sheet.png`:
  one row per set, sprites in mask order, each labelled with its mask.
- **Docstring.** Update the module docstring for the new outputs and orientations.

### 2. Config schema

- **New field.** In `ImprovementConfigParser.h`:

  ```cpp
  enum class SpriteTileLayout_t { Edges, Blob };

  // World-map tile set: one sprite per neighbor mask. Patterns contain "{mask}"; an empty
  // pattern means no art on that surface.
  struct OccupantSpriteTiles_t
  {
      SpriteTileLayout_t layout = SpriteTileLayout_t::Edges;
      std::string land;
      std::string sea;
  };
  ```

  `ImprovementConfig_t` gains `std::optional<OccupantSpriteTiles_t> spriteTiles`.
- **JSON.** `"sprite_tiles": {"layout": "edges" | "blob", "land": "...{mask}...", "sea": "..."}`.
  - `layout` is required and parsed with magic_enum.
  - At least one of `land` / `sea`; each must contain `{mask}`.
  - An entry may not have both `sprite_paths` and `sprite_tiles`.

### 3. `TileAutotile` (new: `include/ui/TileAutotile.h`, `src/ui/TileAutotile.cpp`)

```cpp
// Edges: 4-bit mask over the diamond's NE, SE, SW, NW edge neighbors. Blob: 8-bit mask
// clockwise from the N corner, corner bits kept only when both edges beside them are set.
// Rows off the map never match; x wraps.
std::uint8_t ResolveTileMask(SpriteTileLayout_t layout, const Tile& rTile, const WorldMap& rMap,
                             const std::function<bool(const Tile& rNeighbor)>& matches);
```

### 4. Tile layers

- **New layer types.** Add `TileLayerType_t::Landmark` after `Rockiness` and
  `TileLayerType_t::River` after `Vegetation`.
- **Landmark** resolves to the config id of the tile's terrain feature tagged `landmark`
  (landmarks exclude each other, so there is at most one).
- **River** resolves to `TileLayerContent::k_River` (`"river"`) when the tile has a river.

### 5. `TileRenderer`

- **Tiled occupants.** When an occupant has `spriteTiles`, the sprite path is the tile
  surface's pattern with `{mask}` replaced by `ResolveTileMask`. The rule is "water or at least
  as wet" for the Moisture layer and "same occupant" for every other layer. Without a map the mask
  is 0. Tiled sprites draw at the full tile rect with the layer's existing tint (fungus stays
  untinted).
- **Moisture.** Draw one base sprite: the Arid occupant's `sprite_paths` variant on arid
  tiles, otherwise the Moist or Wet tile. Delete the arid → moist → wet stack.
  `DrawProceduralMoisture_` stays the fallback when no sprite draws.
- **Vegetation.** Forest and fungus draw as tiles at the full rect; delete the fungus inset.
- **Landmark.** Draw it like any other layer. The trailing terrain-feature loop skips
  features tagged `landmark`.
- **River.**
  - Draw the River layer's tile.
  - When no sprite draws, draw a half-segment from the tile centre to each connected edge's
    midpoint; with no connections, draw a short cross.
  - The coast still draws once the layers pass Vegetation, so it sits under the river.
- **Style.** Move `river_color` and `river_line_thickness_ratio` from `world_display` to
  `tile_renderer` (struct, parser, `config/ui/style.json`, `tests/fixtures/ui/style.json`).

### 6. Retire the old paths

- **WorldDisplay.** Delete `WorldDisplay::RenderRivers_` and its call.
- **Edge insets.** Delete `MatchMoistureTierEdges` and `MatchFungusEdges`.
  `MatchRockinessEdges`, `MatchSeaLandformEdges` and `DestRectForEdgeInsets` stay.

### 7. Configs

- **`config/terrain.json`:**
  - `Moist`: `{"layout": "blob", "land": "assets/sprites/landforms/moist/{mask}.png"}`, and `Wet` the same with `wet/`.
  - `Fungus`: blob, with `land` → `fungus_land/{mask}.png` and `sea` → `fungus_sea/{mask}.png`.
  - `River`: `{"layout": "edges", "land": "assets/sprites/landforms/river/{mask}.png"}`.
  - `MonsoonJungle`: blob, with `land` → `jungle/{mask}.png`.

  Each replaces that entry's `sprite_paths`.
- **`config/improvements.json`:** `Forest`: edges, with `land` → `forest/{mask}.png`.
- **Fixtures** mirror this under `tests/fixtures/sprites/`. Add `MonsoonJungle` (tagged
  `landmark`) to the fixture terrain if it is absent.

### 8. Tests

**`TileAutotileTests`:**
- No matching neighbor: mask 0.
- Each edge neighbor sets its own bit, in both layouts.
- A lone corner neighbor sets nothing, and keeps its bit once both adjacent edges match.
- x wraps; off-map rows never match.

**Parser:**
- Both layouts parse.
- A missing `{mask}` is rejected, by name.
- An unknown layout is rejected.
- `sprite_paths` together with `sprite_tiles` is rejected.

**Layers:**
- A tagged landmark fills Landmark.
- A river tile fills River.
- Neither appears otherwise.

**`TileRendererTests`:**
- A moist tile next to arid land draws `moist/<mask>` with that edge's bit.
- A wet tile surrounded by wet draws `wet/0`.
- An arid tile draws the arid sprite and nothing from `moist/`.
- Forest connects to forest neighbors (`forest/<mask>`).
- Fungus uses `fungus_land/` on land and `fungus_sea/` at sea.
- Jungle draws through the Landmark layer and not again in the feature loop.
- A river draws `river/<GetRiverConnections>`; with the sprite missing, it draws the
  fallback lines.
- Draw order is base, landmark, vegetation, coast, river, improvement sprites.

**Requirement changes:**
- Remove the moisture-tier and sea-fungus tests in `TileSpriteEdgeInsetTests`.
- The moisture sprite assertion in "occupant sprite_paths draws a scaled tinted sprite" now
  expects the full tile rect.
- Renderer tests that stub Moist or Fungus through `spritePaths` stub their tile patterns
  instead.

### 9. Docs

- **`map-system.md`:** layer order and diagram (Landmark, River), and `sprite_tiles`.
- **`graphics-system.md`:**
  - TileRenderer bullets: tile sets, single moisture base, rivers.
  - WorldDisplay: rivers are no longer a separate pass.
  - Extractor outputs.
- **`smac-terrain-textures.md`:**
  - The blob reduction and the two SMAC table errors.
  - The orientation table.

### 10. Local cleanup

Delete the stale raw cells under `assets/sprites/landforms/`: `moist_*`, `wet_*`, `forest_*`,
`fungus_land_*`, `fungus_sea_*` and `river_*`.

## Verification

- `./bd test`.
- `extract_terrain.py --contact-sheet`:
  - Each blend row starts with a full cell (mask 0) and ends with the fully surrounded cell
    (mask 255).
  - Edge rows connect toward the edges their mask names.
- In game:
  - Moisture changes blend at real boundaries only, with no fade edges inside uniform areas.
  - Forest, fungus and jungle patches have organic edges.
  - Rivers run unbroken across tiles and over the coast.
