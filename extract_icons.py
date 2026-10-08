#!/usr/bin/env python3
"""
Convert SMAC tech / facility / project PCX icons to PNG under assets/sprites/.

The original game assets are not redistributable, so this script rips them from a
local SMAC installation at setup time.

Usage:
    python extract_icons.py
    python extract_icons.py --game-dir "/path/to/Sid Meier's Alpha Centauri"

Output (gitignored under assets/sprites/):
    techs/tech000.png … tech089.png
    facilities/fac000.png … fac038.png
    facilities/xfac034.png … xfac041.png
    projects/proj000.png … proj036.png

Facility PCX indices follow classic alpha.txt order (orbitals = fac033–fac036), not
alphax.txt insertion indices. SMAX extras that share those slots ship as xfac*.
Palette index 255 is the transparent key (magenta on techs; purple on fac/proj).
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")

from extract_pcx_common import clear_transparent_rgb

_LOCAL_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
_WINDOWS_GAME_DIR = Path(r"C:\Program Files (x86)\Alpha Centauri")
DEFAULT_GAME_DIR = _LOCAL_GAME_DIR if _LOCAL_GAME_DIR.is_dir() else _WINDOWS_GAME_DIR
DEFAULT_ASSET_ROOT = Path("assets")

TRANSPARENT_INDEX = 255

# (install subdirectory, output subdirectory under sprites/)
ICON_FAMILIES = (
    ("techs", "techs"),
    ("facs", "facilities"),
    ("projs", "projects"),
)


def to_rgba(sprite: Image.Image, *, keyed: bool) -> Image.Image:
    if not keyed:
        return sprite.convert("RGBA")
    rgba = sprite.convert("RGBA")
    alpha = sprite.point(lambda index: 0 if index == TRANSPARENT_INDEX else 255, mode="L")
    rgba.putalpha(alpha)
    return clear_transparent_rgb(rgba)


def convert_family(
    game_dir: Path,
    install_subdir: str,
    out_subdir: Path,
    *,
    keyed: bool,
) -> list[Path]:
    source_dir = game_dir / install_subdir
    if not source_dir.is_dir():
        raise FileNotFoundError(f"Missing icon directory: {source_dir}")

    written: list[Path] = []
    for source in sorted(source_dir.glob("*.pcx")):
        sheet = Image.open(source)
        if sheet.mode != "P":
            raise ValueError(f"{source} is mode {sheet.mode}, expected paletted P")
        rgba = to_rgba(sheet, keyed=keyed)
        destination = out_subdir / f"{source.stem}.png"
        destination.parent.mkdir(parents=True, exist_ok=True)
        rgba.save(destination)
        written.append(destination)
    return written


def extract_icons(game_dir: Path, asset_root: Path, *, keyed: bool) -> dict[str, list[Path]]:
    if not game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {game_dir}")

    sprites_root = asset_root / "sprites"
    by_family: dict[str, list[Path]] = {}
    for install_subdir, out_name in ICON_FAMILIES:
        written = convert_family(
            game_dir, install_subdir, sprites_root / out_name, keyed=keyed
        )
        by_family[out_name] = written
    return by_family


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
        help="keep palette index 255 opaque",
    )
    args = parser.parse_args(argv)

    by_family = extract_icons(args.game_dir, args.out, keyed=not args.no_transparency)
    total = sum(len(paths) for paths in by_family.values())
    print(f"icons: {total} sprites -> {args.out / 'sprites'}")
    for name, paths in by_family.items():
        print(f"  {name}: {len(paths)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, OSError) as error:
        sys.exit(str(error))
