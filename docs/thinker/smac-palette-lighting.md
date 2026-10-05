# SMAC palette, terrain lighting and fog (`palette.pcx`)

How Sid Meier's Alpha Centauri colours the map. Reverse-engineered from `terranx.exe` (GOG
build, image base `0x400000`) with Thinker's names. This is **stock SMAC behavior**; our port
is described in [graphics-system.md](../architecture/graphics-system.md). Sheet layouts are in
[smac-terrain-textures.md](smac-terrain-textures.md), coastlines in
[smac-coastline-rainfall.md](smac-coastline-rainfall.md).

## Summary

- The map uses one 256-colour palette, loaded from `palette.pcx` and shown as is. In a
  screenshot of the running game, 99.5% of the window's pixels are exact `palette.pcx`
  colours.
- Shading never blends colours. It adds a small signed offset to each texel's **palette
  index**. The terrain colours are laid out as ramps from light to dark, so +1 is one step
  darker in the same hue (about 10% luminance).
- **Land:** per-vertex slope lighting from −4 to +4. Flat ground gets 0, so it shows the
  sheet colours exactly as painted.
- **Water:** per-vertex depth shade from 0 to 37 along the blue ramp. Shades of 16 and up
  switch from the shelf texture to the deep texture.
- **Fog of war:** tiles outside current sight get shade +2, and every other scanline is
  painted palette index 65, a pale grey-teal (137, 166, 166).

## palette.pcx

`load_palette` (`0x451A10`) reads the `Gamma Correction` preference (default 100, scaled by
0.01) and loads palette entries 10–245 from `PALETTE.PCX`. Windows keeps 0–9 and 246–255.
A gamma other than 1.0 rewrites each entry as `255 · (c / 255)^(1 / γ)` (`0x5C95E0`); the
stock `Alpha Centauri.ini` has no gamma key.

Only the file's palette is read. Its pixels are the artist's chart: an index grid, the
palette laid out as 16-colour ramps, ramps added later, and the player/border colour slots.

`texture.pcx` carries its own palette. It matches `palette.pcx` at every index the terrain
cells use (4–11, 20–27, 36–51, 60–70, 84–95, 104–107, 116–122, 132–134, 148–184); its
other entries are placeholder grey. Luminance along two of those ramps:

| Ramp | Luminance, index by index |
|------|---------------------------|
| 36–51 (arid, moist browns) | 172 154 137 119 102 90 85 80 75 69 64 58 52 47 41 35 |
| 148–184 (water) | 149 135 124 115 120 116 107 101 97 91 88 86 79 77 76 71 67 62 61 55 55 54 52 48 46 48 46 43 43 41 40 37 34 32 30 28 26 |

## Shading is an index offset

`Texture_draw` (`0x61B1F0`) and `Texture_draw_2` (`0x6233C0`) rasterize with per-vertex
shades interpolated across the polygon (8.8 fixed point). The pixel loop is

```text
mov al, [texel]     ; palette index from the texture
add al, ch          ; + integer part of the interpolated shade
mov [dest], al
```

Shades are clamped to ±256 (`0x6972CC` / `0x6972D0`).

## Land lighting

- `MapWin_compute_lighting_table` (`0x470420`) fills four 7×7 tables of signed bytes at
  `0x7F6680` (49 bytes apart), one per tile corner. An entry's two inputs `a, b` are relative
  vertex altitudes from −3 to 3. Its value is `−3·(s + t) / √(s² + t²)`, truncated, where
  `(s, t)` is `(a, b)`, `(−b, a)`, `(−a, −b)` or `(b, −a)` for corners 0–3. That is the same
  slope rotated a quarter turn per corner, lit from one fixed direction. Values run −4…+4;
  flat ground is 0.
- `MapWin_get_brighting` (`0x470550`) looks up a tile corner and caches it (+0x80) in the
  map window's per-tile cache. Ocean tiles cache 0.
- `MapWin_get_point_light` (`0x470790`) gives a vertex the average of the eight corner values
  from the four tiles that meet there (the tile centre averages its own four). Crater,
  volcano and canyon tiles then add a per-vertex offset (`0x685684` → `0x685A24`, `0x685B60`,
  `0x7F6750`; values such as +3 and −2).
- `MapWin_gen_terrain_poly` (`0x4632D0`) uses these as land vertex shades when the tile is
  lit. Unlit tiles get the constant shade 2 (`0x686354`). The same pass raises each land
  vertex on screen by its altitude (`MapWin_get_alt`), which is where the map's relief
  comes from.

## Water depth

Each water vertex looks up the tile's altitude-detail byte (`alt_get_ocean_detail`,
`0x462190`; 0–79, sea level near 60) in the table at `0x6861F0`:

| Detail | 0–5 | 6–9 | 10–14 | 15–29 | 30–31 | 32 | 33–57 | 58+ |
|--------|-----|-----|-------|-------|-------|----|-------|-----|
| Shade | 37 | 36 | 35 | 25 | 16 | 14 | 13 → 1, about one step per two details | 0 |

If all four vertex shades are below 16, the tile draws the shelf texture (indices 154–156)
with those shades. Otherwise it draws the deep texture (162–170) with `shade − 16`. Both
slide down the water ramp, so the sea darkens smoothly from teal to deep blue. On a captured
SMAC map, water pixels average index 172, against 162 for the raw textures.

## Fog of war

Lighting is on for tiles in current sight. It is also on for every tile when the fog
preference (`GameMorePreferences` bit 0) is off or the map is zoomed far out. A tile outside
sight has every vertex at shade 2 and is drawn by `Texture_draw_2` in mode `0x41`: alternate
scanlines are painted solid palette 65 (137, 166, 166) wherever the texel is not the key.
Remembered terrain therefore looks slightly darker and hazed with pale lines, not dimmed.

## Colour tables

`colortables_init` (`0x423570`) builds 256-entry remap tables with
`Palette_create_table(_from_color)`: shadow, spotlight, reddish, redtint, bluetint and fade.
They are cached as `*.tmp` in the game folder. It also builds the faction colour tables
(`colortables_init_faction`, `0x6F077C`) that the territory-border pass paints with. The
terrain and water passes use neither remap table.

## Key addresses

| VA | What |
|----|------|
| `0x451A10` | `load_palette` |
| `0x5C95E0` | Gamma correction of the palette |
| `0x61B1F0` / `0x6233C0` | `Texture_draw` / `Texture_draw_2` (index + shade) |
| `0x470420` | `MapWin_compute_lighting_table` |
| `0x470550` | `MapWin_get_brighting` |
| `0x470790` | `MapWin_get_point_light` |
| `0x7F6680` | Corner lighting tables, 4 × 49 bytes |
| `0x6861F0` | Water depth → shade table |
| `0x686354` | Shade for unlit tiles (2) |
| `0x423570` | `colortables_init` |
