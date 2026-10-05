# SMAC terrain textures (`texture.pcx`, `ter1.pcx`)

What Sid Meier's Alpha Centauri cuts from its terrain sheets and how it picks a cell per
tile. Crops come from emulating `load_terrain` (`0x451C50`) in `terranx.exe` (GOG build);
selection rules come from `MapWin_gen_terrain_poly` (`0x4632D0`). Names follow Thinker.
Coastlines are covered separately in [smac-coastline-rainfall.md](smac-coastline-rainfall.md).

## texture.pcx

56×56 cells on 57 px grids, listed row-major (four cells per row unless noted).

| Set | First cell | Cells | Destination | Picked by |
|-----|-----------|-------|-------------|-----------|
| Rolling / rocky overlays | (1, 1) | 4 | `0x76E9E0` | Rockiness |
| Dunes (`LM_DUNES`) | (229, 1) | 1 | `0x7AC220` | Landmark replaces the base |
| Arid base | (1, 58) | 1 | `0x799E48` | Rainfall 0 |
| Moist bases | (1, 115) | 16 | `0x799EB8` | Rainfall 1, blend table |
| Wet bases | (1, 343) | 16 | `0x799738` | Rainfall 2, blend table |
| Ocean shelf / deep | (280, 79) / (280, 136) | 2 | `0x7A7820` | Water pass |
| River | (280, 259) | 16 | `0x7A7AA0` | Edge mask of `BIT_RIVER` neighbors |
| Land fungus | (280, 516) | 15 | `0x776A80` | Blend table on fungus neighbors |
| Sea fungus | (508, 516) | 15 | `0x777110` | Same, when the tile is ocean |
| Forest | (526, 6) | 16 | `0x78A758` | Edge mask of `BIT_FOREST` neighbors |
| Jungle (`LM_JUNGLE`) | (526, 259) | 15 | `0x789C28` | Blend table on jungle neighbors |
| Volcano (`LM_VOLCANO`) | (1, 628) | 3 | `0x78A340` | Landmark art index |
| Crater (`LM_CRATER`) | (1, 685) | 3 | `0x788100` | Landmark art index |
| Mesa (`LM_MESA`) | (768, 15) | 8 | `0x7797F8` | Landmark art index |
| Farm | (775, 219), 3 per row | 9 | `0x799238` | Column = rainfall, row = per-tile random 0–2 |
| Roads | (775, 395), 3 per row | 9 | `0x792218` | Hub + one cell per direction |
| Mag tubes | (775, 566), 3 per row | 9 | `0x798E08` | Hub + one cell per direction |

Two framed cells at (1, 571) and (58, 571) are drawn outside the terrain pass. Keys are
indices 0 and 255; overlays (forest, fungus, river, rocky, jungle) sit on that key.

### Edge masks (forest, river)

Bits 0–3 are the edge-sharing neighbors NE, SE, SW, NW (SMAC deltas `0x66EF28` /
`0x66EF3C`; on our square grid (0, −1), (+1, 0), (0, +1), (−1, 0)). A bit is set when that
neighbor has the same feature; the mask is the cell index, so cell 15 connects on all four
edges. No rotation.

### Blend table (moist, wet, fungus, jungle)

An 8-neighbor mask (bit `k` = SMAC direction `(k − 1) & 7`, i.e. N, NE, E, SE, S, SW, W,
NW) indexes 256 two-byte entries at `0x685484`: shape 0–14 and rotation 0–3. The rotation
turns the cell's UVs in quarter steps. Only 47 (shape, rotation) pairs occur. Corner
neighbors alone never change the result; they refine shapes once edges are set.

The table is the standard 47-blob reduction: a corner bit counts only when both edge bits
beside it are set. That holds for 254 of the 256 entries. Masks 202 and 218 are data errors:
they select a four-edge shape although only three edges are set. Reducing the mask first
avoids both.

- **Moist / wet:** a bit is set for a neighbor that is ocean or at least as wet as the
  tile (`0x463DBD`: altitude below sea level, or rainfall ≥ the tile's own). Shape 0 is an
  isolated patch fading out on every side; shape 14 is the plain base. The 16th cell is
  unused.
- **Fungus / jungle:** a bit is set for a neighbor with the same feature (fungus counts
  on shelf or land). Shape 0 is an isolated patch, shape 14 fully surrounded.

### Orientation

Checked against the cell art: blend cell 1 reaches only the top side, cell 2 top + right,
cell 9 all four sides. Forest/river cell 1 reaches the right side, 2 the bottom, 4 the left,
8 the top. The diamond's screen corners take these texture corners:

| Cells | W | N | E | S |
|-------|---|---|---|---|
| Blend sets and plain cells (arid, rolling, rocky, water, shelf, ocean), rotation 0 | BL | TL | TR | BR |
| Edge sets (forest, river) and the coast | TL | TR | BR | BL |

Rotation `r` shifts the blend row: screen corner `k` (W = 0 … S = 3) takes the rotation-0
texture corner of `(k + r) & 3`.

## ter1.pcx objects

Sprites are 100×62: a 100×50 footprint diamond on the bottom 50 rows and 12 rows of art
above it. Purple 253 is the key; dark purple 252 outlines the footprint for the artist,
and `load_terrain` turns both into the transparent index after loading.

- **Tile bonuses** (`0x75B230`, 3 rows × 4 columns from (1, 253), 101 × 63 px steps):
  rows are nutrients, minerals, energy; columns are sea, sea, land, land. The draw at
  `0x465E70` uses index `(type − 1)·4 + isOcean·2 + ((x/2 + y) & 1)`.

Everything `load_terrain` cuts from the sheet, and what draws it:

| Sprites | First cell | Drawn for |
|---------|-----------|-----------|
| Monolith | (304, 1) | `BIT_MONOLITH` |
| Mining platform, mine | (506, 64), (607, 64) | `BIT_MINE`, sea / land |
| Tidal harness, solar collector | (506, 127), (607, 127) | `BIT_SOLAR`, sea / land |
| Kelp | (607, 190) | `BIT_FARM` on sea |
| Borehole cluster | (708, 190) | `LM_BOREHOLE` |
| Condenser, echelon mirror, thermal borehole | (506, 253), (607, 253), (708, 253) | `BIT_CONDENSER`, `BIT_ECH_MIRROR`, `BIT_THERMAL_BORE` |
| Bunker, airbase, sensor | (506, 316), (607, 316), (708, 316) | `BIT_BUNKER`, `BIT_AIRBASE`, `BIT_SENSOR` |
| Geothermal, uranium | (822, 190), (822, 253) | `LM_GEOTHERMAL`, `LM_URANIUM` |
| Tile bonuses | (1, 253), 3 × 4 | See above |
| Supply pods | (418, 379), 6 across | Pods on the tile |
| Soil enricher, farm structures | (822, 453), (923, 453), 4 rows | `BIT_SOIL_ENRICHER`, `BIT_FARM` on land; row = clamp(nutrient yield − 1, 0, 3) |
| Manifold Nexus pieces | (478, 632), 3 × 2 | `LM_NEXUS`, by landmark art index |
| Temperature overlay | (1, 447), (1, 579), 3 columns | Draw flag `0x8`: white / yellow / orange by the tile's temperature bits |
| Rainfall overlay | (1, 510), (1, 642), (1, 705), 3 columns | Draw flag `0x10`: light / mid / dark green by rainfall; the triangle row when both overlays are on |

The farm structures sit on the `texture.pcx` farm ground. Column (620, 453) and
(506, 190) are only shown by the Datalinks and the tutorial. The isometric composite at
(220, 447) shows how the Nexus pieces assemble. The textured diamonds at the top right
and the labels are artist reference and are not loaded.

## ter1wreck.pcx and glow.pcx

`ter1wreck.pcx` uses the same 100×62 layout:

- **Unity wreckage** (`LM_UNITY`): 15 pieces from (26, 21) in a 3-wide grid and 4 more from
  (329, 147), picked by the landmark art index. The composite at (130, 410) shows them
  assembled.
- **Fossil field** (`LM_FOSSIL`): 6 pieces from (329, 21).

`load_terrain` also asks for `glow.pcx` (two sprites at (1, 1) and (102, 1)), which only the
Datalinks draw. The GOG install has no such file.

Colours and shading for all of these are covered in
[smac-palette-lighting.md](smac-palette-lighting.md).
