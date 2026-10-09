# SMAC UI chrome sheets

Crops and roles for Phase 4 UI chrome extraction (`extract_ui.py`). Measured on the GOG
`terranx.exe` install sheets. Palette index **255** is the transparent key. UI text is solid
RGBA (GDI `SetTextColor`), not terrain `palette.pcx`.

## Whole sheets

| Source | Size | Output |
|---|---|---|
| `console.pcx` | 800×257 | `console.png` (default, side bars) + `console_nobar.png` |
| `console2.pcx` | 1024×257 | `console2.png` (default, side bars) + `console2_nobar.png` |
| `console_x.pcx` / `console_x2.pcx` | 800×600 | whole sheet + crops under `chrome/console_x/` / `chrome/console_x2/` |
| `text.pcx` | 800×600 | `assets/ui/chrome/text.png` (dialog sheet; not a glyph atlas) |
| `artbox0.pcx`–`artbox24.pcx` | 420×520 | `assets/ui/artboxes/artboxNN.png` |
| `*_sm.pcx` | varies | `assets/ui/thumbs/<stem>.png` |

## `console2.pcx` world console (1024×257)

Drawn as a **single** native-size strip via `layouts.console` (fixed placement: `align`
`[0.5, 1.0]` bottom-centered). Default extract output pads opaque black on both sides out
to a 3840-wide canvas (`extract_ui.py` `CONSOLE_SIDE_COVER_WIDTH`), starting at the
top-silhouette valley so gutters match the map seam; excess pad clips at the window edge.
Plain strips are `console*_nobar.png` — switch the variant `sprite` and set
`sprite_offset_x` to 0 to use them. Layout `width`/`height` stay the art size
(1024×257 / 800×257); `sprite_offset_x` is the negative left pad so the art still seats on
the layout origin. Corner towers sit above the center frame; each variant’s `map_overlap`
(45px — that valley from extract) extends the map to the valley with towers drawn over it.
`console_backdrop_color` fills from the map bottom to the window bottom before the sprite
draws. Do not slice the strip into per-panel backgrounds — the metal frame is continuous.
Content views use `world_view.console_layouts` (ratios of the placed console rect):

| Window | Box in sheet (x0,y0)–(x1,y1) | `console_layouts` key | Role |
|---|---|---|---|
| Left (upper) | (5,57)–(244,163) | `unit` | Selected unit |
| Left (lower) | (5,163)–(244,251) | `location` | Location / tile preview |
| Center upper | (263,67)–(762,171) | `info` | Info / status |
| Center lower | (263,189)–(762,251) | `stack` | Unit stack |
| Right | (780,57)–(1019,251) | `minimap` | Minimap |

`console.pcx` (800×257) is the same chrome for 800-wide mode; shipping UI uses `console2`.

## `console_x.pcx` / `console_x2.pcx` widget crops (800×600)

These are **widget sheets** (cyan guide boxes, palette index **254**), not bottom-band
layout composites like `console2`. Interiors are cropped with the same guided convention
as `Cursor.pcx` (guide edges exclusive). Whole-sheet PNGs remain for RE reference.

Shared naming under `assets/ui/chrome/console_x/` and `…/console_x2/`:

| Name pattern | Role |
|---|---|
| `bar_a0`–`bar_a4`, `bar_b0`–`bar_b4` | Horizontal button / status bars (two columns × five intensity steps) |
| `map_topo_*`, `map_weather_*`, `map_flag_*`, `map_cycle_*` | Map-layer toggles; suffix `0`–`2` = state rows |
| `accent_0`–`accent_2` | Thin solid accent strips |
| `rail_0`–`rail_3` | Short vertical rail / scroll pieces |
| `panel_l*`, `panel_m*`, `panel_r*` | Metallic console bracket segments (glow variants by row) |
| `vstrip_0`–`vstrip_3` | Tall vertical border strips (right edge of sheet) |

`console_x` vs `console_x2`: same left-column widgets; bracket/`vstrip` guide boxes differ
(widths and x origins). `console_x` also has a lower-left **green** guide block (index
**247**) used as 9-slice scaffolding; Phase 4 extracts those as whole widgets only and
clears index 247 to transparent (interior scaffold lines are not paint):

| Name | Guide box | Role |
|---|---|---|
| `nine_panel_0`–`nine_panel_2` | (0,409)–(51,428/449/470) | Blue ribbed panels (9-slice grid inside) |
| `orb_purple_0`–`orb_purple_2` | 21×21 guide cells at y 472 | Purple orb icons |
| `orb_white_0`–`orb_white_2` | 21×21 guide cells at y 493 | White orb icons |

Exact coordinates live in `extract_ui.py` (`CONSOLE_X_CROPS`, `CONSOLE_X2_CROPS`).

## `iface.pcx` (1024×768)

Content sits in three vertical bands (keyed magenta outside): roughly x 0–320, 332–882,
895–1015. Wired panel frames this phase:

| Name | Box | Size | Role |
|---|---|---|---|
| `panel_frame` | (40,40)–(360,397) | 320×357 | Generic modal / research / list frame |

Further button and widget cells remain for later RE; unused crops may be added without
wiring.

## `Cursor.pcx` (1024×768)

Cyan guide index **149** on a 33 px grid (32×32 interiors), same convention as faction
sheets. Non-empty cells in the first rows:

| Name | Guide box (x0,y0)–(x1,y1) | Interior | Role |
|---|---|---|---|
| `arrow` | (33,0)–(66,33) | 32×32 | Default pointer |
| `arrow_alt` | (66,0)–(99,33) | 32×32 | Alternate pointer |
| `wait` | (99,0)–(132,33) | 32×32 | Wait / busy |
| `airdrop` | (132,0)–(165,33) | 32×32 | Parachute (airdrop targeting) |
| `invalid` | (165,0)–(198,33) | 32×32 | Prohibited |
| `target` | (297,0)–(330,33) | 32×32 | Crosshair (bombard targeting) |
| `bombard` | (363,0)–(396,33) | 32×32 | Missile pair (bombard) |

Hotspots (pixels from cell top-left): airdrop `(16, 28)` (crate), bombard/target `(16, 16)`
(centre).

## `text.pcx` dialog crop

| Name | Box | Size | Role |
|---|---|---|---|
| `dialog_panel` | (195,80)–(605,387) | 410×307 | List / notice popup backdrop |

## Color-blind pack

`Color Blind Palette/` copies are out of Phase 4.
