# SMAC UI chrome sheets

Crops and roles for Phase 4 UI chrome extraction (`extract_ui.py`). Measured on the GOG
`terranx.exe` install sheets. Palette index **255** is the transparent key. UI text is solid
RGBA (GDI `SetTextColor`), not terrain `palette.pcx`.

## Whole sheets

| Source | Size | Output |
|---|---|---|
| `console.pcx` | 800×257 | `assets/ui/chrome/console.png` |
| `console2.pcx` | 1024×257 | `assets/ui/chrome/console2.png` |
| `console_x.pcx` / `console_x2.pcx` | 800×600 | whole sheet + crops under `chrome/console_x/` / `chrome/console_x2/` |
| `text.pcx` | 800×600 | `assets/ui/chrome/text.png` (dialog sheet; not a glyph atlas) |
| `artbox0.pcx`–`artbox24.pcx` | 420×520 | `assets/ui/artboxes/artboxNN.png` |
| `*_sm.pcx` | varies | `assets/ui/thumbs/<stem>.png` |

## `console2.pcx` world console (1024×257)

Drawn as a **single** native-size strip (`assets/ui/chrome/console2.png`) at `layouts.console`.
The map uses `layouts.map`, which ends on a solid line at the console’s top. Shipping ratios
target the default 1280×900 window so the console rect is exactly 1024×257 and centered
(`[0.1, 0.7144, 0.8, 0.2856]`). Retune those ratios if the window size changes. A full-width
`console_backdrop_color` fill under that line occludes anything below before the sprite draws.
Do not slice the strip into per-panel backgrounds — the metal frame is continuous. Content
views use `world_view.console_layouts` (ratios of the console layout rect):

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
