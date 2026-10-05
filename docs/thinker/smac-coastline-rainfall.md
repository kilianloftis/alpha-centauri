# SMAC coastline rendering (`Rainfall.pcx` coast pass)

How Sid Meier's Alpha Centauri draws land/water shores. Reverse-engineered from
`terranx.exe` (GOG build, image base `0x400000`) with Thinker's names for engine
functions and globals (`induktio/thinker`, `engine.cpp`). This is **stock SMAC
behavior**; our port is described in
[graphics-system.md](../architecture/graphics-system.md).

## Summary

- There is no coast sprite sheet. A coast is **ocean painted onto the edge of a land
  tile**: ocean tiles draw only water, and every land tile next to water overdraws
  part of itself with ocean texture and a thin shore band.
- The shape comes from one 56×56 template in `Rainfall.pcx`, painted with 53
  placeholder palette indices. A code table turns each placeholder into "keep land",
  "ocean" or a shore colour, depending on which neighbors are water.
- A land tile is handled as four corner quadrants. Each quadrant looks at three
  neighbors (two edge neighbors and one corner neighbor), so its water state is a
  3-bit mask; the mask selects the code-table column.

## Where it happens

`MapWin_gen_terrain_poly` (`0x4632D0`) draws one tile. In order:

1. Base land texture, rockiness overlays, landmark art (volcano, mesa, …), jungle blend
   (`0x464F10`), forest (`BIT_FOREST`, `0x465029`), fungus (`BIT_FUNGUS`, `0x4650E3`).
2. Territory border (`0x4651C7` loop, drawn at `0x465340`).
3. **Coast** (`0x46535A` loop, `Texture_draw_coast_2` at `0x46547C`). Land tiles only.
4. River (`BIT_RIVER`, from `0x46549C`), then improvement sprites (mine, solar, …).

## Quadrant water masks

Land branch at `0x463BE0`. For each of the eight neighbors that is ocean (altitude
`climate >> 5` below `ALT_SHORE_LINE` = 3):

```text
k = dir >> 1                          dir: 0 NE, 1 E, 2 SE, 3 S, 4 SW, 5 W, 6 NW, 7 N
odd dir  (E, S, W, N corner):   quad[(k - 2) & 3] |= 2
even dir (NE, SE, SW, NW edge): quad[(k + 1) & 3] |= 4
                                quad[(k - 2) & 3] |= 1
```

SMAC's map uses packed coordinates: `(±1, ±1)` neighbors share a diamond **edge**,
`(±2, 0)` / `(0, ±2)` neighbors touch a diamond **corner** (deltas at `0x66EF50` /
`0x66EF74`). Quadrants are the regions around the corners: 0 = W, 1 = N, 2 = E, 3 = S.
Per quadrant, going clockwise around the diamond:

| Quadrant | bit 1 (edge before the corner) | bit 2 (corner) | bit 4 (edge after the corner) |
|----------|------|---|------|
| 0 W      | SW   | W | NW   |
| 1 N      | NW   | N | NE   |
| 2 E      | NE   | E | SE   |
| 3 S      | SE   | S | SW   |

Neighbors off the top or bottom of the map are skipped (land); x wraps unless the map
is flat. Quadrants with mask 0 are not drawn.

## Template and geometry

`load_terrain` (`0x451C50`) crops `Rainfall.pcx` (2, 333) 113×56 and splits it into two
56×56 textures, `0x76E8A0` (x 0–55 of the crop) and `0x76E910` (x 57–112). Only
`0x76E8A0` is ever drawn. Template pixels are rewritten to their ordinal in the
placeholder list (anything else becomes ordinal 0).

Each quadrant samples only its own quarter of the template (UVs at `0x6860F0`), with
the same UVs into the ocean texture:

| Quadrant | Template texels (u, v) |
|----------|------------------------|
| W        | 0–27, 0–27 (top-left)  |
| N        | 28–55, 0–27 (top-right) |
| E        | 28–55, 28–55 (bottom-right) |
| S        | 0–27, 28–55 (bottom-left) |

The texture square maps onto the diamond rotated 45°: square top-left → W corner,
top-right → N, bottom-right → E, bottom-left → S. The square's top edge is the
diamond's NW edge. Quadrant screen vertices come from `0x685EA0` / `0x685EE0`.

## Code table

`Texture_draw_coast` (`0x61EAD0`) and `Texture_draw_coast_2` (`0x623F80`) rasterize
the ocean texture `0x7A7820` through the template. Per pixel:

```text
c = lut[template ordinal]
c == 0  → leave the pixel (land shows)
c == 1  → ocean texel, plus the per-vertex depth shade
c >= 2  → write palette index c (not shaded)
```

`Texture_draw_coast_2` also implements the fog scanline effect (`a9 = 0x41`).

The 53-entry `lut` is rebuilt per quadrant (`0x465379`): row `i` is
`code[i][mask]`, and codes 2–5 become `sample[code + 2]`, i.e. the second row of the
3×4 grid sampled from `Rainfall.pcx` at init (`0x452215`, stored at `0x787E54`):

| Code | Rainfall.pcx pixel | Shipped palette index |
|------|--------------------|-----------------------|
| 2    | (130, 361)         | 68 (dark wet edge)    |
| 3    | (144, 361)         | 11 (cliff, dark)      |
| 4    | (158, 361)         | 10 (cliff)            |
| 5    | (172, 361)         | 7 (cliff, light)      |

The other two sample rows are not used by the coast pass.

Placeholder indices (`0x6846F8`) and codes (`0x684730`, 53 rows × 8 bytes). Each row
lists the codes for masks 0 through 7:

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

### Alternate islet shape

When a quadrant's mask is 7 and `(y - x) & 2` holds for the tile, the lut is patched
(`0x4653AC`): rows 201 and 193–196 become 0, and each of the groups 58/59/60/62,
146/147/149/151 and 23/25/27/29 gets `sample[7], sample[6], sample[5], sample[4]`
(7, 10, 11, 68 on the shipped sheet). With SMAC x = gx − gy and y = gx + gy this is
an odd row on a square grid.

## Ocean texture

`load_terrain` extracts the ocean textures from `texture.pcx`: shelf (280, 79) 56×56
into `0x7A7820` and deep (280, 136) into `0x7A7890`. The cells sit between guide rows
at y = 78, 135 and 192. The coast pass always uses the shelf texture.

## Palettes

`texture.pcx` and the global `palette.pcx` agree at every index the terrain cells use
and at 68, 11, 10 and 7. `Rainfall.pcx`'s own palette differs at 132–134, which some
terrain cells use, so coast colours should be resolved through `texture.pcx`'s palette.
The copy under `Color Blind Palette/` has a differently painted template; the game-root
`Rainfall.pcx` is the reference.

## Other Rainfall.pcx content

| Crop | Destination | Use |
|------|-------------|-----|
| (1, 52) 100×100 | `0x7ACBB8` | Base-window art, drawn by `BaseWin_init` (`0x41F337`) |
| (2, 333) 113×56 | `0x76E8A0`, `0x76E910` | Coast templates (only the first is drawn) |
| (2, 254) 60×60 | `0x78DD80` | Territory-border template |

The border template is keyed by its own 9-index list (211, 208, 41, 143, 116, 151, 206,
95, 192). The border pass (`0x4651C7`) checks the eight neighbors for the same
ocean/land domain, the same region byte and a different `whose_territory`, and paints
the matching template segments in the owner's colour. It is unrelated to coasts.

The rest of the sheet (moisture diagrams, the "Evolution of a Coastline" strip, the
numbered swatches) is artist documentation.

## texture.pcx strip at x ≥ 768

The brown 4-column strip on the right of `texture.pcx` (`0x7797F8`, from (768, 15)) is
drawn only for tiles with the `LM_MESA` (`0x100`) landmark. It is not shore or cliff
art.

## Key addresses

| VA | What |
|----|------|
| `0x4632D0` | `MapWin_gen_terrain_poly` |
| `0x463BE0` | Land branch: quadrant water masks |
| `0x46452C` | Water pass: quadrant geometry and ocean depth per vertex |
| `0x46535A` | Coast loop over the four quadrants |
| `0x4653AC` | Alternate islet lut patch |
| `0x451C50` | `load_terrain` |
| `0x452195` | `Rainfall.pcx` load, crops, sample grid |
| `0x61EAD0` / `0x623F80` | `Texture_draw_coast` / `Texture_draw_coast_2` |
| `0x6846F8` | 53 placeholder indices |
| `0x684730` | Code table, 53 × 8 |
| `0x6860F0` | Quadrant template UVs |
| `0x685EA0` / `0x685EE0` | Quadrant vertex factors |
| `0x787E54` | `Rainfall.pcx` sample grid (12 bytes) |
| `0x66EF50` / `0x66EF74` | Neighbor Δx / Δy |
