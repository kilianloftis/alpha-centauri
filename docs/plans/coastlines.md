# Coastlines: SMAC's Rainfall.pcx coast pass

Based on `main` at `d569332` (Tile edge blending), after the uncommitted first attempt is discarded (step 1).

## Context

### What SMAC draws

Verified against `terranx.exe` (GOG build), using Thinker's names for engine functions. `MapWin_gen_terrain_poly` (`0x4632D0`) draws one tile.

- **Only land tiles get a coast.** Ocean tiles get none. The coast is an overlay on a land tile. It is drawn after the tile's base texture, rocks, landmark art, jungle, forest, fungus and territory border. It is drawn before rivers, improvement sprites (mines, solar collectors) and units.
- **Four corner quadrants.** A land tile is split into the four regions around its diamond corners (W, N, E, S). Each quadrant reads three neighbors and builds a 3-bit water mask (`0x463BE0`):
  - `1`: the edge neighbor counter-clockwise of the corner
  - `2`: the corner neighbor
  - `4`: the edge neighbor clockwise of the corner

  A neighbor is water when its altitude is below the shore line. Rows off the map count as land, and x wraps. A quadrant with mask 0 gets no coast.
- **One template.** `Rainfall.pcx` (2, 333), 56×56, is painted with 53 placeholder palette indices. Each quadrant samples only its own quarter of it. Texels 0–27 and 28–55 split as follows:

  | Quadrant | Template quarter |
  |---|---|
  | W | top-left |
  | N | top-right |
  | E | bottom-right |
  | S | bottom-left |

  The square maps onto the diamond with its top-left corner at W, top-right at N, bottom-right at E and bottom-left at S. The square's top edge is therefore the diamond's NW edge.
- **Lookup.** The quadrant's mask selects a column of the code table below, and the template pixel's placeholder selects the row. Codes:
  - `0`: keep the land pixel.
  - `1`: draw the ocean texture at the same texel. The texture is the `texture.pcx` shelf cell at (280, 79), and it is depth-shaded like ocean tiles.
  - `2`, `3`, `4`, `5`: draw the colour sampled from `Rainfall.pcx` at (130, 361), (144, 361), (158, 361) and (172, 361) respectively. On the shipped sheet these are palette indices 68, 11, 10 and 7: a dark wet edge and a brown cliff ramp. These colours are not shaded.
- **Alternate islet art.** SMAC switches mask-7 quadrants to a second shape when `(y − x) & 2` holds in its coordinates. On our grid that is an odd row (SMAC x = gx − gy, y = gx + gy). The alternate shape changes these rows:
  - Rows 201 and 193–196 become `0`.
  - Each of the groups 58/59/60/62, 146/147/149/151 and 23/25/27/29 gets the code-5, code-4, code-3 and code-2 colours, in that order. On the shipped sheet that is 7, 10, 11, 68.
- **Take every colour from `texture.pcx`'s palette.** It matches the game's `palette.pcx` at every index the terrain cells use, and at 68, 11, 10 and 7. `Rainfall.pcx`'s palette does not: it differs at 132–134, which some terrain cells use.
- **Use the game-root `Rainfall.pcx`.** The copy in `Color Blind Palette/` has a differently painted template.

Placeholder indices (`0x6846F8`) and the code table (`0x684730`). Each row lists the codes for masks 0 through 7:

```text
  5: 00000000   148: 01010101    67: 01010111   131: 00001111   180: 00011111
201: 00000001     7: 05050505     9: 04040404    11: 03030303    13: 02010202
 39: 00005555    41: 00004444    43: 00003333    45: 00002222   193: 00000005
194: 00000004   195: 00000003   196: 00000002    58: 05050501    59: 04040401
 60: 03030301    62: 02020201   146: 00005551   147: 00004441   149: 00003331
151: 00002221    23: 00050051    25: 00040041    27: 00030031    29: 00020021
 71: 01010151    73: 01010141    75: 01010131    77: 01010121   103: 00051111
105: 00041111   107: 00031111   109: 00021111   136: 05010511   137: 04010411
139: 03010311   140: 02010211   116: 00015511   118: 00014411   119: 00013311
121: 00012211    88: 05012011    89: 04013011    90: 03014011   165: 05515011
166: 04414011    96: 02010311   144: 00015311
```

Template pixels that are not in this list behave like row 5: always `0`.

### Our grid

`MapViewport` projects `screenX ∝ gx − gy` and `screenY ∝ gx + gy`. So an orthogonal neighbor shares a diamond edge, and a diagonal neighbor touches a diamond corner.

| Corner | bit 1 (edge, counter-clockwise) | bit 2 (corner) | bit 4 (edge, clockwise) |
|--------|---------|----------|---------|
| West   | (0, +1) | (−1, +1) | (−1, 0) |
| North  | (−1, 0) | (−1, −1) | (0, −1) |
| East   | (0, −1) | (+1, −1) | (+1, 0) |
| South  | (+1, 0) | (+1, +1) | (0, +1) |

### Design

- **Bake the overlays offline.** All inputs are static (template, table, sampled colours, shelf texture). `extract_terrain.py` therefore bakes every quadrant case into RGBA sprites, and the runtime only picks sprites and tints. No palettes, index sidecars or LUTs exist at runtime.
- **Two sprites per case.** One sprite holds the water pixels (code 1) and one holds the shore pixels (codes 2–5). The water is tinted to match the adjacent water tiles, and the shore takes the land tile's tint.
- **Every overlay is tile-sized.** Each sprite has the 2:1 tile AABB and is drawn at the tile's own rect, like every other tile sprite. The four quadrants then share one texel grid, so they cannot seam against each other at any zoom.

## Changes

### 1. Discard the first attempt

The first attempt is uncommitted. Restore these tracked files to `HEAD`:

- `docs/architecture/graphics-system.md`
- `docs/architecture/map-system.md`
- `extract_terrain.py`
- `include/ui/TileSpriteEdgeInset.h`
- `src/ui/TileSpriteEdgeInset.cpp`
- `src/ui/TileRenderer.cpp`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/ui/TileSpriteEdgeInsetTests.cpp`

Delete `include/ui/CoastPaletteRemap.h`, `src/ui/CoastPaletteRemap.cpp` and `tests/ui/CoastPaletteRemapTests.cpp`.

Delete its local extraction output (`assets/sprites/` is git-ignored):

- `assets/sprites/coastline/`
- `assets/sprites/cliffs/`
- `assets/sprites/_probe/`
- `assets/sprites/_cliff_probe/`
- `assets/sprites/_rainfall_grid_contact.png`
- `assets/sprites/smac_palette.rgb`
- every `assets/sprites/landforms/*.idx`

### 2. Research doc

Rewrite `docs/thinker/smac-coastline-rainfall.md` from the Context section above. Add these references:

- **Coast routines.**
  - Quadrant masks: land branch at `0x463BE0`.
  - Quadrant geometry: water pass `0x46452C`.
  - Coast draw loop: `0x46535A`.
  - `Texture_draw_coast` (`0x61EAD0`) and `Texture_draw_coast_2` (`0x623F80`): per pixel `c = lut[template]`. `0` skips the pixel, `1` writes the shaded ocean texel, and anything else writes `c`. `Texture_draw_coast_2` also draws the fog scanline variant.
  - Quadrant UVs: `0x6860F0`. Quadrant vertex factors: `0x685EA0` / `0x685EE0`.
- **`load_terrain` (`0x451C50`).**
  - Ocean textures: shelf (280, 79) into `0x7A7820`, and deep (280, 136) into `0x7A7890`.
  - Rainfall crops:
    - (1, 52) 100×100 into `0x7ACBB8`: base-window art, drawn by `BaseWin_init`.
    - (2, 333) 113×56 into two 56×56 templates. `0x76E8A0` is the coast template; `0x76E910` is loaded but never drawn.
    - (2, 254) 60×60 into `0x78DD80`: the territory-border template, keyed by a separate 9-index list.
- **Territory borders (`0x4651C7`).** This 8-neighbor pass draws a border segment toward neighbors that have the same domain and region but a different `whose_territory`. It is not part of the coast.
- **Draw order around the coast.** Before it: jungle blend (`0x464F10`), forest (`BIT_FOREST`, `0x465029`), fungus (`BIT_FUNGUS`, `0x4650E3`) and the border (`0x465340`). After it: river (`BIT_RIVER`, from `0x46549C`).
- **The `texture.pcx` strip at x ≥ 768.** This is landmark art, drawn for `LM_MESA` (`0x100`) tiles. It is not coast art.

### 3. `extract_terrain.py`

- **Ocean crops.** Move the `water` and `ocean_shelf` regions to (280, 79, 336, 135). The current y = 80 crop takes in the guide row at y = 135.
- **Remove the `sprites/cliffs/*` regions.** They are the mesa strip, and nothing draws them.
- **Load the game-root `Rainfall.pcx`.** It is 640×480; extend `load_sheet` to take the expected size.
- **SMAC data.** Add `COAST_CODES` (placeholder index → codes for masks 0–7) and the alternate-shape sets `COAST_ALT_LAND` / `COAST_ALT_RAMPS` as constants copied from the tables above, with their `terranx.exe` addresses in a comment. The shore colours are not constants: read them from the four `Rainfall.pcx` pixels at y = 361.
- **Bake the coast sprites.** Write two sprites per corner and case to `sprites/coast/`, named `water_<corner>_<case>.png` and `shore_<corner>_<case>.png`:
  - `<corner>` is `w`, `n`, `e` or `s`.
  - `<case>` is `1`–`7` or `7_alt`.
  - That makes 64 files. Write every case, even when a sprite comes out empty: mask 2 has no water and only a few shore pixels.
- **Bake geometry.** Each sprite is 112×56.
  - For pixel (px, py), let `a = (px + 0.5) / 112` and `b = (py + 0.5) / 56 − 0.5`. Then `s = a − b` and `t = a + b`. `s` runs from the W corner toward N, and `t` from W toward S.
  - The texel is `u = floor(56·s)`, `v = floor(56·t)`, clamped to 0–55.
  - A pixel belongs to a corner when it is in that corner's quarter:
    - W: `s < ½`, `t < ½`
    - N: `s ≥ ½`, `t < ½`
    - E: `s ≥ ½`, `t ≥ ½`
    - S: `s < ½`, `t ≥ ½`
  - **Bleed.** Extend each quarter by 1/56 past the two sides that lie on the tile's outer edge:
    - W: `s` and `t` down to −1/56
    - N: `s` up to 1 + 1/56, `t` down to −1/56
    - E: `s` and `t` up to 1 + 1/56
    - S: `s` down to −1/56, `t` up to 1 + 1/56

    The bleed covers the jagged edge of the land sprite's diamond mask, so no land shows through where the overlay draws water up to the edge.
  - Look up the template placeholder at (u, v) and apply the case's codes, including the alternate shape for `7_alt`. Code 1 takes the shelf cell's RGB at (u, v). Codes 2–5 take the sampled index's RGB from `texture.pcx`'s palette. Both are opaque, and every other pixel gets alpha 0.
- **Contact sheet.** With `--contact-sheet`, also write `sprites/_coast_contact_sheet.png`. It has one row per corner and one column per case, and each cell composites that corner's water and shore sprites over a flat land diamond.
- **Docstring.** Update the module docstring for the new outputs.

### 4. `CoastOverlay` (new: `include/ui/CoastOverlay.h`, `src/ui/CoastOverlay.cpp`)

```cpp
enum class CoastCorner_t { West, North, East, South };

struct CoastCornerArt_t
{
    CoastCorner_t corner = CoastCorner_t::West;
    // 1 = edge neighbor counter-clockwise of the corner, 2 = corner neighbor,
    // 4 = edge neighbor clockwise of it. 0 = no coast in this quadrant.
    std::uint8_t waterMask = 0;
    // SMAC's second all-water shape; only with waterMask == 7, on odd rows.
    bool bAlternate = false;
};

struct CoastOverlay_t
{
    std::array<CoastCornerArt_t, 4> corners{}; // indexed by CoastCorner_t
    std::vector<std::reference_wrapper<const Tile>> waterNeighbors; // of the eight
};

// Water tile → all masks 0 and no neighbors. Rows off the map count as land; x wraps.
CoastOverlay_t ResolveCoastOverlay(const Tile& rTile, const WorldMap& rMap);
```

Neighbor deltas follow the table in Context. Water means `Tile::IsWater()`.

### 5. `TileRenderer`

- **When to draw.** When `pMap` is set and the tile is land, draw the coast once the Vegetation layer is done and before the Road layer.
- **Water tint.** Average, component-wise, `SpriteTint_(neighbor, bFogged)` over `waterNeighbors`, using the centre tile's `bFogged`.
- **Draw order.** First draw `water_<corner>_<case>.png` with the water tint for each corner whose mask is non-zero. Then draw `shore_<corner>_<case>.png` with the tile's `tint` for the same corners. Both use the tile's full rect (`x`, `y`, `width`, `height`).
- **Missing files** draw nothing, through `TryDrawSprite_`.
- **Sprite directory.** Paths are `<coast_sprite_dir>/<part>_<corner>_<case>.png`. Add `coast_sprite_dir` to `TileRendererStyle_t` and `ParseTileRendererStyle_` by hand, like `airdrop_cursor_path`. The key is required, and an empty string is rejected at load.
  - `config/ui/style.json`: `"assets/sprites/coast"`
  - `tests/fixtures/ui/style.json`: `"tests/fixtures/sprites/coast"`

### 6. Tests

Add `src/ui/CoastOverlay.cpp` and `tests/ui/CoastOverlayTests.cpp` to the CMake lists.

**`tests/ui/CoastOverlayTests.cpp`** (`WorldFixture` maps):

- Land surrounded by land: every mask is 0, and there are no water neighbors.
- A water tile next to land: every mask is 0.
- Each edge-sharing water neighbor sets bit 4 on one corner and bit 1 on the next:

  | Water at | Bit 4 on | Bit 1 on |
  |---|---|---|
  | (0, −1) | North | East |
  | (+1, 0) | East | South |
  | (0, +1) | South | West |
  | (−1, 0) | West | North |

- Each corner-touching water neighbor sets bit 2 on that corner only:
  - (−1, −1): North
  - (+1, −1): East
  - (+1, +1): South
  - (−1, +1): West
- A one-tile islet: all four masks are 7, and `waterNeighbors` holds all eight neighbors.
- A land tile on row 0 whose only water would be off the map: every mask is 0.
- A tile at x = 0 with water at x = width − 1 on the same row: West 4, North 1.
- An islet on an odd row is alternate on every corner, and on an even row on none. A mask other than 7 on an odd row is never alternate.

**`tests/ui/TileRendererTests.cpp`** (stub PNGs under the fixture `coast_sprite_dir`):

- A land tile with water at (0, −1) draws `water_n_4`, `water_e_1`, then `shore_n_4`, `shore_e_1`, all at the tile rect. It draws no other coast sprites.
- Coast sprites come after the moisture and fungus sprites, and before the tile's improvement sprites.
- Overlay water uses the tint of the water neighbor's own landform sprite. To test this, give the fixture `OceanShelf` a `sprite_paths` entry under `tests/fixtures/sprites/` (and `Mine` one for the ordering case). With two water neighbors at different depths, the overlay water uses the average of their tints.
- Shore sprites use the same tint as the tile's land sprites. When the tile is fogged, both tints are dimmed.
- One-tile islets draw `_7_alt` art on odd rows and `_7` art on even rows.
- No coast sprites are drawn without a map, for water tiles, or for inland tiles.

### 7. `DestRectForEdgeInsets`

Inset layer sprites must stay inside the tile diamond; otherwise a moisture tier inset from drier land overhangs its opposite, water-facing edge and shows as a land sliver past the coast. Fit the scaled diamond in the tile's own unit square (`s` from the W corner toward N, `t` from W toward S):

- Each unmatched edge pads its side by `insetRatio`; the scale is `min(1 − padSW − padNE, 1 − padNW − padSE)`.
- Per axis, sit flush against the matched side when only the other side pads; otherwise center between the pads.

Add tests: for every edge combination and several ratios the sprite's corners stay inside the tile diamond, and a single inset edge leaves the opposite matched edge on the tile edge.

### 8. Docs

- **`graphics-system.md`.**
  - WorldDisplay *Tile drawing*: land tiles next to water get the baked coast overlay, and `extract_terrain.py` also writes `sprites/coast/`.
  - TileRenderer component: add a **Coast** bullet naming `CoastOverlay`, the four corner masks, the draw position (after Vegetation, before Road), the tints and `coast_sprite_dir`.
  - Drop "cliff skirts" from the follow-on note.
- **`map-system.md`.**
  - Elevation paragraph: replace "cliff-edge compositing from `texture.pcx` is deferred" with a pointer to the coast overlay and the research doc.
  - Delete the TODO "Neighbor-based cliff/slope sprites from `assets/sprites/cliffs/`".

## Verification

- `./bd test`.
- Run `extract_terrain.py --contact-sheet` and check `_coast_contact_sheet.png` cell by cell:
  - Mask 1 is water along the counter-clockwise edge, and mask 4 along the clockwise edge.
  - Mask 2 is land, apart from a notch at the corner.
  - Mask 5 has water on both edges and a land bridge to the corner.
  - Mask 7 is water around a small rounded cap, and `7_alt` is the squarer cap.
- In game, check a coastline:
  - The cliff band runs unbroken across tile seams.
  - No land shows along water edges.
  - Land tiles that touch only at a corner are joined by a thin causeway.
  - One-tile islands are rounded, and the islet shape alternates by row.
