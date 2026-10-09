# SMAC UI typography

Face and colour notes for UI text drawn over chrome. UI text is solid RGBA via GDI
`SetTextColor`, **not** terrain `palette.pcx` / `DrawTileSprite`.

## Face

- Primary UI face in classic SMAC: **Arial Narrow** (`arialn.ttf`).
- Our runtime loads the first usable path in `graphics.font_paths`, preferring
  `assets/ui/fonts/arialn.ttf` (from `extract_fonts.py`), then system DejaVu/Liberation.
- Bold/italic copies may exist on disk; runtime still uses one face this phase.

## Sample sizes (1024×768)

Approximate on-screen sizes used when auditing `style.json` ratios:

| Surface | ~px |
|---|---|
| World info panel lines | 14–16 |
| Selected-unit / location body | 12–14 |
| Popup header | 18–20 |
| Popup row / notice body | 14–16 |
| Base production / buildings header | ~12% of panel height |
| Map base name | ~25% of tile width |

## Sample colours (RGB)

Measured against SMAC chrome (paletted UI under GDI text), not terrain ramps:

| Role | RGB | Notes |
|---|---|---|
| Body / entry on dark console | 255, 255, 255 | Most dashboard labels |
| Muted / empty state | 180, 180, 180 | “No unit”, hints |
| Popup header | 255, 220, 120 | Warm gold on dialog chrome |
| Popup / notice body | 255, 255, 255 | |
| Research label | 200, 220, 255 | Cool accent on panel frame |
| Map base name fallback | 255, 255, 0 | Overridden by faction `faction_text_color_*` when present |

## World-map base name anchor

SMAC places the base name under the base sprite, not at the tile footprint top-left.

- Seat for the sprite: `FootprintOrigin` (corner-mean), height
  `tileWidth · isoHeight · (1 + base_sprite_overhang_ratio)`.
- Label origin: `FootprintOrigin + (tileWidth · base_name_offset_x_ratio,
  tileWidth · base_name_offset_y_ratio)` with `base_name_offset_y_ratio` past the sprite
  bottom (~0.62 with overhang 0.21).
- Still truncated to `base_name_width_ratio`; faction label colour unchanged.
