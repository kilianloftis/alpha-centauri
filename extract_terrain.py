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

texture.pcx uses palette indices 0 and 255 as magenta-family transparency keys
(faction sheets use 255 only). ter1.pcx uses index 253 for its purple key.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")


# --------------------------------------------------------------------------- #
# Layout
# --------------------------------------------------------------------------- #

# texture.pcx: left rainfall block is a 4-column grid with 1px guides every 57px.
TEXTURE_CELL = 56
TEXTURE_STRIDE = 57

# texture.pcx transparency (both indices appear as “empty” on the sheet).
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

    @property
    def box(self) -> tuple[int, int, int, int]:
        return (self.x0, self.y0, self.x1, self.y1)


def _grid_cell(col: int, row: int) -> tuple[int, int, int, int]:
    x0 = 1 + col * TEXTURE_STRIDE
    y0 = 1 + row * TEXTURE_STRIDE
    return (x0, y0, x0 + TEXTURE_CELL, y0 + TEXTURE_CELL)


def _texture_regions() -> list[Region]:
    arid = _grid_cell(0, 1)
    moist = _grid_cell(0, 6)
    wet = _grid_cell(0, 9)
    rolling = _grid_cell(0, 4)
    rocky = _grid_cell(1, 0)

    # Xenofungus autotile grid: guides at x=279/336/…, first interior cell ~y=260.
    fungus = (280, 260, 336, 316)
    # Ocean tiles right of the rainfall grid.
    shelf = (280, 80, 280 + TEXTURE_CELL, 80 + TEXTURE_CELL)
    deep = (280, 136, 280 + TEXTURE_CELL, 136 + TEXTURE_CELL)

    return [
        Region("sprites/landforms/flat", *arid),
        Region("sprites/landforms/arid", *arid),
        Region("sprites/landforms/moist", *moist),
        Region("sprites/landforms/wet", *wet),
        Region("sprites/landforms/rolling", *rolling),
        Region("sprites/landforms/rocky", *rocky),
        # Rainy/green cell doubles as forest until a dedicated forest crop is refined.
        Region("sprites/landforms/forest", *wet),
        Region("sprites/landforms/water", *shelf),
        Region("sprites/landforms/ocean_shelf", *shelf),
        Region("sprites/landforms/ocean", *deep),
        Region("sprites/landforms/fungus", *fungus),
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


def extract_regions(
    sheets: dict[str, Image.Image],
    out_root: Path,
    *,
    keyed: bool,
) -> list[Path]:
    written: list[Path] = []
    for region in REGIONS:
        sheet = sheets[region.sheet]
        sprite = to_rgba(sheet.crop(region.box), region.key_indices, keyed=keyed)
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
    args = parser.parse_args(argv)

    if not args.game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {args.game_dir}")

    sheets = {
        "texture": load_sheet(find_pcx(args.game_dir, "texture")),
        "ter1": load_sheet(find_pcx(args.game_dir, "ter1")),
    }
    written = extract_regions(sheets, args.out, keyed=not args.no_transparency)
    if args.contact_sheet:
        write_contact_sheet(sheets, args.out)
    print(f"terrain: {len(written)} sprites -> {args.out}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as error:
        sys.exit(str(error))
