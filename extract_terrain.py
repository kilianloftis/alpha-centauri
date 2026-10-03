#!/usr/bin/env python3
"""
Extract map terrain sprites from Sid Meier's Alpha Centauri texture.pcx / ter1.pcx.

The original game assets are not redistributable, so this script rips them from a
local SMAC installation at setup time. Wire into extract_all.py when that
orchestrator lands (see graphics assets roadmap).

Usage:
    python extract_terrain.py
    python extract_terrain.py --game-dir "/path/to/Sid Meier's Alpha Centauri"
    python extract_terrain.py --contact-sheet

texture.pcx uses palette indices 0 and 255 as transparency keys (empty sheet pixels).
Fungus art is a sparse overlay on that key: keep both indices transparent, then recolor
the remaining teal detail toward SMAC pink (the sheet stores fungus cyan/teal).

Rainfall-grid layout (col, row) — left 4×N block:
  (0,0)/(2,0) rolling overlays; (1,0)/(3,0) rocky overlays;
  (0,1) arid base; rows 2–5 moist bases (16 cells); rows 6–9 wet bases (16 cells).
Flat has no dedicated art (moisture base alone). Rolling/rocky are keyed overlays.
ter1.pcx uses index 253 for its purple key.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    from PIL import Image, ImageChops, ImageDraw
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")

# --------------------------------------------------------------------------- #
# Layout
# --------------------------------------------------------------------------- #

# texture.pcx: left rainfall block is a 4-column grid with 1px guides every 57px.
TEXTURE_CELL = 56
TEXTURE_STRIDE = 57

# texture.pcx transparency (both indices are empty on the sheet).
TEXTURE_KEY_INDICES = frozenset({0, 255})
# ter1.pcx purple key (+ spare magenta indices sometimes present).
TER1_KEY_INDICES = frozenset({0, 253, 255})


@dataclass(frozen=True)
class Region:
    """Crop box in sheet pixel coordinates (inclusive-exclusive PIL style)."""

    path: str  # relative to assets root, no extension
    x0: int
    y0: int
    x1: int
    y1: int
    sheet: str = "texture"  # texture | ter1
    key_indices: frozenset[int] = field(default_factory=lambda: TEXTURE_KEY_INDICES)
    # Landform squares are masked to the iso diamond AABB.
    diamond_mask: bool = True
    # Recolor teal xenofungus sheet pixels toward in-game pink.
    remap_fungus_pink: bool = False

    @property
    def box(self) -> tuple[int, int, int, int]:
        return (self.x0, self.y0, self.x1, self.y1)


def _grid_cell(col: int, row: int) -> tuple[int, int, int, int]:
    x0 = 1 + col * TEXTURE_STRIDE
    y0 = 1 + row * TEXTURE_STRIDE
    return (x0, y0, x0 + TEXTURE_CELL, y0 + TEXTURE_CELL)


def _band_variants(stem: str, row0: int, row1: int) -> list[Region]:
    """Row-major cells in [row0, row1] × cols 0..3 → stem_0.png …"""
    regions: list[Region] = []
    index = 0
    for row in range(row0, row1 + 1):
        for col in range(4):
            regions.append(Region(f"sprites/landforms/{stem}_{index}", *_grid_cell(col, row)))
            index += 1
    return regions


def _texture_regions() -> list[Region]:
    arid = _grid_cell(0, 1)
    # Stand-in forest crop from a wet cell until dedicated forest art is mapped.
    wet_forest = _grid_cell(0, 6)

    # Xenofungus autotile grid: guides at x=279/336/…, first interior cell ~y=260.
    fungus = (280, 260, 336, 316)
    # Ocean tiles right of the rainfall grid.
    shelf = (280, 80, 280 + TEXTURE_CELL, 80 + TEXTURE_CELL)
    deep = (280, 136, 280 + TEXTURE_CELL, 136 + TEXTURE_CELL)

    return [
        Region("sprites/landforms/arid", *arid),
        # Moisture bases: moist rows 2–5, wet rows 6–9 (16 variants each).
        *_band_variants("moist", 2, 5),
        *_band_variants("wet", 6, 9),
        # Rockiness overlays on row 0 (magenta-keyed, not full bases).
        Region("sprites/landforms/rolling_0", *_grid_cell(0, 0)),
        Region("sprites/landforms/rolling_1", *_grid_cell(2, 0)),
        Region("sprites/landforms/rocky_0", *_grid_cell(1, 0)),
        Region("sprites/landforms/rocky_1", *_grid_cell(3, 0)),
        Region("sprites/landforms/forest", *wet_forest),
        Region("sprites/landforms/water", *shelf),
        Region("sprites/landforms/ocean_shelf", *shelf),
        Region("sprites/landforms/ocean", *deep),
        # Xenofungus overlay: key empty pixels, recolor teal detail to pink.
        Region(
            "sprites/landforms/fungus",
            *fungus,
            remap_fungus_pink=True,
        ),
        # Stand-in until the river autotile cluster is fully mapped; WorldDisplay still
        # draws river centerlines. Uses the deep-ocean cell as a blue water cue.
        Region("sprites/landforms/river", *deep),
        # Cliff / slope edges — extracted for later neighbor compositing; unused in Phase 1.
        Region("sprites/cliffs/left", 780, 20, 860, 100),
        Region("sprites/cliffs/right", 780, 100, 860, 180),
        Region("sprites/cliffs/corner", 880, 20, 960, 100),
        Region("sprites/cliffs/corner_cap", 880, 100, 960, 180),
    ]


def _ter1_regions() -> list[Region]:
    key = TER1_KEY_INDICES
    # Resource grid: horizontal guides at y=252/315/378/441, vertical at x=0/101/202/303.
    return [
        Region("sprites/tile_bonuses/monolith", 310, 28, 400, 165, sheet="ter1", key_indices=key),
        Region(
            "sprites/tile_bonuses/nutrient_rich_soil",
            1,
            253,
            101,
            315,
            sheet="ter1",
            key_indices=key,
        ),
        Region(
            "sprites/tile_bonuses/mineral_deposit",
            1,
            316,
            101,
            378,
            sheet="ter1",
            key_indices=key,
        ),
        Region(
            "sprites/tile_bonuses/energy_vein",
            1,
            379,
            101,
            441,
            sheet="ter1",
            key_indices=key,
        ),
    ]


REGIONS: tuple[Region, ...] = tuple(_texture_regions() + _ter1_regions())


# --------------------------------------------------------------------------- #
# Extraction
# --------------------------------------------------------------------------- #


def find_pcx(game_dir: Path, stem: str) -> Path:
    wanted = f"{stem.lower()}.pcx"
    for candidate in game_dir.iterdir():
        if candidate.is_file() and candidate.name.lower() == wanted:
            return candidate
    raise FileNotFoundError(f"{stem}.pcx not found in {game_dir}")


def load_sheet(pcx_path: Path, expected_size: tuple[int, int] = (1024, 768)) -> Image.Image:
    image = Image.open(pcx_path)
    if image.mode != "P":
        raise ValueError(f"{pcx_path.name}: expected a paletted PCX, got mode {image.mode!r}")
    if image.size != expected_size:
        raise ValueError(f"{pcx_path.name}: expected {expected_size}, got {image.size}")
    return image


def to_rgba(sprite: Image.Image, key_indices: frozenset[int], *, keyed: bool) -> Image.Image:
    if not keyed:
        return sprite.convert("RGBA")
    rgba = sprite.convert("RGBA")
    alpha = sprite.point(lambda index: 0 if index in key_indices else 255, mode="L")
    rgba.putalpha(alpha)
    return rgba


def apply_diamond_mask(rgba: Image.Image) -> Image.Image:
    """Keep pixels inside a diamond covering the full AABB (2:1 isometric footprint).

    Square texture.pcx crops are stretched into a W×W/2 diamond on the map; masking
    here so corners stay transparent when drawn into that AABB.
    """
    width, height = rgba.size
    mask = Image.new("L", (width, height), 0)
    ImageDraw.Draw(mask).polygon(
        [
            (width // 2, 0),
            (width - 1, height // 2),
            (width // 2, height - 1),
            (0, height // 2),
        ],
        fill=255,
    )
    red, green, blue, alpha = rgba.split()
    return Image.merge("RGBA", (red, green, blue, ImageChops.multiply(alpha, mask)))


def clear_transparent_rgb(rgba: Image.Image) -> Image.Image:
    """Zero RGB on alpha=0 pixels so chroma-key leftovers cannot leak when blending."""
    pixels = rgba.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0 and (red or green or blue):
                pixels[x, y] = (0, 0, 0, 0)
    return rgba


def remap_fungus_pink(rgba: Image.Image) -> Image.Image:
    """Recolor sheet teal/cyan fungus detail toward SMAC pink; leave brown tip accents."""
    pixels = rgba.load()
    width, height = rgba.size
    for y in range(height):
        for x in range(width):
            red, green, blue, alpha = pixels[x, y]
            if alpha == 0:
                pixels[x, y] = (0, 0, 0, 0)
                continue
            # Rust/brown tip accents on the sheet — keep as-is.
            if red > green and red > blue and green < 100:
                continue
            value = max(red, green, blue) / 255.0
            pixels[x, y] = (
                min(255, int(220 * value + 40)),
                min(255, int(40 * value + 20)),
                min(255, int(180 * value + 40)),
                alpha,
            )
    return rgba


def extract_regions(
    sheets: dict[str, Image.Image],
    out_root: Path,
    *,
    keyed: bool,
    diamond_mask: bool,
) -> list[Path]:
    written: list[Path] = []
    for region in REGIONS:
        sheet = sheets[region.sheet]
        sprite = to_rgba(sheet.crop(region.box), region.key_indices, keyed=keyed)
        if region.remap_fungus_pink:
            sprite = remap_fungus_pink(sprite)
        if diamond_mask and region.diamond_mask:
            sprite = apply_diamond_mask(sprite)
        sprite = clear_transparent_rgb(sprite)
        destination = out_root / f"{region.path}.png"
        destination.parent.mkdir(parents=True, exist_ok=True)
        sprite.save(destination)
        written.append(destination)
    return written


def write_contact_sheet(sheets: dict[str, Image.Image], out_root: Path) -> Path:
    texture = sheets["texture"].convert("RGB")
    draw = ImageDraw.Draw(texture)
    for region in REGIONS:
        if region.sheet != "texture":
            continue
        left, top, right, bottom = region.box
        draw.rectangle([left, top, right - 1, bottom - 1], outline=(0, 255, 0))
        draw.text((left + 2, top + 2), Path(region.path).name, fill=(0, 255, 0))
    destination = out_root / "sprites/_terrain_contact_sheet.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    texture.save(destination)
    return destination


DEFAULT_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
DEFAULT_ASSET_ROOT = Path("assets")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--game-dir",
        type=Path,
        default=DEFAULT_GAME_DIR,
        help=f"SMAC install (default: {DEFAULT_GAME_DIR})",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_ASSET_ROOT,
        help=f"asset root (default: {DEFAULT_ASSET_ROOT})",
    )
    parser.add_argument(
        "--no-transparency",
        action="store_true",
        help="keep the key colour opaque",
    )
    parser.add_argument(
        "--contact-sheet",
        action="store_true",
        help="also write sprites/_terrain_contact_sheet.png outlining texture crops",
    )
    parser.add_argument(
        "--no-diamond-mask",
        action="store_true",
        help="keep square crops (default applies a diamond alpha mask for isometric draw)",
    )
    args = parser.parse_args(argv)

    if not args.game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {args.game_dir}")

    sheets = {
        "texture": load_sheet(find_pcx(args.game_dir, "texture")),
        "ter1": load_sheet(find_pcx(args.game_dir, "ter1")),
    }
    written = extract_regions(
        sheets,
        args.out,
        keyed=not args.no_transparency,
        diamond_mask=not args.no_diamond_mask,
    )
    if args.contact_sheet:
        write_contact_sheet(sheets, args.out)
    print(f"terrain: {len(written)} sprites -> {args.out}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as error:
        sys.exit(str(error))
