#!/usr/bin/env python3
"""
Extract SMAC UI chrome, cursors, thumbs, and artboxes into assets/ui/.

The original game assets are not redistributable, so this script rips them from a
local SMAC installation at setup time.

Usage:
    python extract_ui.py
    python extract_ui.py --game-dir "/path/to/Sid Meier's Alpha Centauri"

Output (gitignored under assets/ui/):
    chrome/console.png, console2.png (default: black side bars to cover wide windows)
    chrome/console_nobar.png, console2_nobar.png (plain strips; point style.json here to opt out)
    chrome/console_x*/ crops, text.png, iface crops, dialog_panel
    cursors/<name>.png
    thumbs/*_sm.png
    artboxes/artboxNN.png

Region tables: docs/thinker/smac-ui-chrome.md
Palette index 255 is the transparent key.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
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
CURSOR_GUIDE_INDEX = 149
# console_x*.pcx use cyan guide boxes (not the Cursor/faction index 149).
CONSOLE_X_GUIDE_INDEX = 254


@dataclass(frozen=True)
class Region:
    """Crop box as inclusive guide edges; interior is (x0+1,y0+1)–(x1,y1) when guided."""

    path: str
    x0: int
    y0: int
    x1: int
    y1: int
    guided: bool = False
    # Extra palette indices to treat as transparent after crop (e.g. interior 9-slice guides).
    clear_indices: tuple[int, ...] = ()

    @property
    def box(self) -> tuple[int, int, int, int]:
        if self.guided:
            return (self.x0 + 1, self.y0 + 1, self.x1, self.y1)
        return (self.x0, self.y0, self.x1, self.y1)


# Whole-file converts under chrome/ (stem → relative path without extension).
# console.pcx / console2.pcx are handled separately (plain + side-bar variants).
WHOLE_CHROME = (
    "console_x.pcx",
    "console_x2.pcx",
    "text.pcx",
)

# World console strips: plain art plus a default copy with opaque black side pads so a
# centered console occludes the map in the window gutters (pads clip at the window edge).
CONSOLE_SHEETS = (
    "console.pcx",
    "console2.pcx",
)
# Pad each side so a centered strip covers this window width; wider windows still clip.
CONSOLE_SIDE_COVER_WIDTH = 3840

# console_x2.pcx (800×600): cyan guide index 254. Widget sheet, not a layout band.
CONSOLE_X2_CROPS = (
    Region("chrome/console_x2/bar_a0", 0, 0, 191, 27, guided=True),
    Region("chrome/console_x2/bar_b0", 191, 0, 382, 27, guided=True),
    Region("chrome/console_x2/panel_m0", 383, 0, 548, 61, guided=True),
    Region("chrome/console_x2/panel_r0", 548, 0, 712, 65, guided=True),
    Region("chrome/console_x2/vstrip_0", 712, 0, 728, 530, guided=True),
    Region("chrome/console_x2/vstrip_1", 728, 0, 737, 530, guided=True),
    Region("chrome/console_x2/vstrip_2", 737, 0, 746, 530, guided=True),
    Region("chrome/console_x2/vstrip_3", 746, 0, 762, 530, guided=True),
    Region("chrome/console_x2/bar_a1", 0, 27, 191, 54, guided=True),
    Region("chrome/console_x2/bar_b1", 191, 27, 382, 54, guided=True),
    Region("chrome/console_x2/bar_a2", 0, 54, 191, 81, guided=True),
    Region("chrome/console_x2/bar_b2", 191, 54, 382, 81, guided=True),
    Region("chrome/console_x2/panel_m1", 383, 61, 548, 122, guided=True),
    Region("chrome/console_x2/panel_r1", 548, 65, 712, 130, guided=True),
    Region("chrome/console_x2/bar_a3", 0, 81, 191, 108, guided=True),
    Region("chrome/console_x2/bar_b3", 191, 81, 382, 108, guided=True),
    Region("chrome/console_x2/bar_a4", 0, 108, 191, 135, guided=True),
    Region("chrome/console_x2/bar_b4", 191, 108, 382, 135, guided=True),
    Region("chrome/console_x2/panel_m2", 383, 122, 548, 183, guided=True),
    Region("chrome/console_x2/panel_r2", 548, 130, 712, 195, guided=True),
    Region("chrome/console_x2/map_topo_0", 0, 135, 49, 162, guided=True),
    Region("chrome/console_x2/map_weather_0", 49, 135, 96, 162, guided=True),
    Region("chrome/console_x2/map_flag_0", 96, 135, 143, 162, guided=True),
    Region("chrome/console_x2/map_cycle_0", 143, 135, 193, 162, guided=True),
    Region("chrome/console_x2/panel_l0", 193, 135, 358, 196, guided=True),
    Region("chrome/console_x2/map_topo_1", 0, 162, 49, 189, guided=True),
    Region("chrome/console_x2/map_weather_1", 49, 162, 96, 189, guided=True),
    Region("chrome/console_x2/map_flag_1", 96, 162, 143, 189, guided=True),
    Region("chrome/console_x2/map_cycle_1", 143, 162, 193, 189, guided=True),
    Region("chrome/console_x2/panel_m3", 383, 183, 548, 244, guided=True),
    Region("chrome/console_x2/map_topo_2", 0, 189, 49, 216, guided=True),
    Region("chrome/console_x2/map_weather_2", 49, 189, 96, 216, guided=True),
    Region("chrome/console_x2/map_flag_2", 96, 189, 143, 216, guided=True),
    Region("chrome/console_x2/map_cycle_2", 143, 189, 193, 216, guided=True),
    Region("chrome/console_x2/panel_r3", 548, 195, 712, 260, guided=True),
    Region("chrome/console_x2/panel_l1", 193, 196, 358, 261, guided=True),
    Region("chrome/console_x2/accent_0", 0, 216, 107, 231, guided=True),
    Region("chrome/console_x2/accent_1", 0, 231, 107, 246, guided=True),
    Region("chrome/console_x2/panel_m4", 383, 244, 548, 305, guided=True),
    Region("chrome/console_x2/accent_2", 0, 246, 107, 261, guided=True),
    Region("chrome/console_x2/panel_r4", 548, 260, 712, 325, guided=True),
    Region("chrome/console_x2/panel_l2", 193, 261, 358, 322, guided=True),
    Region("chrome/console_x2/rail_0", 0, 278, 33, 305, guided=True),
    Region("chrome/console_x2/rail_1", 0, 305, 33, 340, guided=True),
    Region("chrome/console_x2/panel_l3", 193, 322, 358, 387, guided=True),
    Region("chrome/console_x2/rail_2", 0, 340, 33, 366, guided=True),
    Region("chrome/console_x2/rail_3", 0, 366, 33, 401, guided=True),
)

# console_x.pcx (800×600): same cyan guide layout as console_x2, plus green (247)
# 9-slice widget boxes in the lower-left (extracted as whole widgets, not 9-slice cells).
CONSOLE_X_CROPS = (
    Region("chrome/console_x/bar_a0", 0, 0, 191, 27, guided=True),
    Region("chrome/console_x/bar_b0", 191, 0, 382, 27, guided=True),
    Region("chrome/console_x/panel_m0", 383, 0, 515, 52, guided=True),
    Region("chrome/console_x/panel_r0", 515, 0, 682, 49, guided=True),
    Region("chrome/console_x/vstrip_0", 682, 0, 698, 530, guided=True),
    Region("chrome/console_x/vstrip_1", 698, 0, 707, 530, guided=True),
    Region("chrome/console_x/vstrip_2", 707, 0, 716, 530, guided=True),
    Region("chrome/console_x/vstrip_3", 716, 0, 732, 530, guided=True),
    Region("chrome/console_x/bar_a1", 0, 27, 191, 54, guided=True),
    Region("chrome/console_x/bar_b1", 191, 27, 382, 54, guided=True),
    Region("chrome/console_x/panel_r1", 515, 49, 682, 98, guided=True),
    Region("chrome/console_x/panel_m1", 383, 52, 515, 104, guided=True),
    Region("chrome/console_x/bar_a2", 0, 54, 191, 81, guided=True),
    Region("chrome/console_x/bar_b2", 191, 54, 382, 81, guided=True),
    Region("chrome/console_x/bar_a3", 0, 81, 191, 108, guided=True),
    Region("chrome/console_x/bar_b3", 191, 81, 382, 108, guided=True),
    Region("chrome/console_x/panel_r2", 515, 98, 682, 147, guided=True),
    Region("chrome/console_x/panel_m2", 383, 104, 515, 156, guided=True),
    Region("chrome/console_x/bar_a4", 0, 108, 191, 135, guided=True),
    Region("chrome/console_x/bar_b4", 191, 108, 382, 135, guided=True),
    Region("chrome/console_x/map_topo_0", 0, 135, 49, 162, guided=True),
    Region("chrome/console_x/map_weather_0", 49, 135, 96, 162, guided=True),
    Region("chrome/console_x/map_flag_0", 96, 135, 143, 162, guided=True),
    Region("chrome/console_x/map_cycle_0", 143, 135, 193, 162, guided=True),
    Region("chrome/console_x/panel_l0", 193, 135, 325, 187, guided=True),
    Region("chrome/console_x/panel_r3", 515, 147, 682, 196, guided=True),
    Region("chrome/console_x/panel_m3", 383, 156, 515, 208, guided=True),
    Region("chrome/console_x/map_topo_1", 0, 162, 49, 189, guided=True),
    Region("chrome/console_x/map_weather_1", 49, 162, 96, 189, guided=True),
    Region("chrome/console_x/map_flag_1", 96, 162, 143, 189, guided=True),
    Region("chrome/console_x/map_cycle_1", 143, 162, 193, 189, guided=True),
    Region("chrome/console_x/panel_l1", 193, 187, 360, 236, guided=True),
    Region("chrome/console_x/map_topo_2", 0, 189, 49, 216, guided=True),
    Region("chrome/console_x/map_weather_2", 49, 189, 96, 216, guided=True),
    Region("chrome/console_x/map_flag_2", 96, 189, 143, 216, guided=True),
    Region("chrome/console_x/map_cycle_2", 143, 189, 193, 216, guided=True),
    Region("chrome/console_x/panel_r4", 515, 196, 682, 245, guided=True),
    Region("chrome/console_x/panel_m4", 383, 208, 515, 260, guided=True),
    Region("chrome/console_x/accent_0", 0, 216, 107, 231, guided=True),
    Region("chrome/console_x/accent_1", 0, 231, 107, 246, guided=True),
    Region("chrome/console_x/panel_l2", 193, 236, 325, 288, guided=True),
    Region("chrome/console_x/accent_2", 0, 246, 107, 261, guided=True),
    Region("chrome/console_x/rail_0", 0, 278, 33, 305, guided=True),
    Region("chrome/console_x/panel_l3", 193, 288, 360, 337, guided=True),
    Region("chrome/console_x/rail_1", 0, 305, 33, 340, guided=True),
    Region("chrome/console_x/rail_2", 0, 340, 33, 366, guided=True),
    Region("chrome/console_x/rail_3", 0, 366, 33, 401, guided=True),
    # Green 247 is the 9-slice scaffold; clear it so crops are paint-only.
    Region("chrome/console_x/nine_panel_0", 0, 409, 51, 428, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/nine_panel_1", 0, 428, 51, 449, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/nine_panel_2", 0, 449, 51, 470, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_purple_0", 0, 472, 21, 493, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_purple_1", 21, 472, 42, 493, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_purple_2", 42, 472, 63, 493, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_white_0", 0, 493, 21, 514, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_white_1", 21, 493, 42, 514, guided=True, clear_indices=(247,)),
    Region("chrome/console_x/orb_white_2", 42, 493, 63, 514, guided=True, clear_indices=(247,)),
)

IFACE_CROPS = (
    Region("chrome/iface/panel_frame", 40, 40, 360, 397),
)

TEXT_CROPS = (
    Region("chrome/dialog_panel", 195, 80, 605, 387),
)

# Cursor.pcx: 33 px guide grid; names for non-empty cells used by the game.
CURSOR_CROPS = (
    Region("cursors/arrow", 33, 0, 66, 33, guided=True),
    Region("cursors/arrow_alt", 66, 0, 99, 33, guided=True),
    Region("cursors/wait", 99, 0, 132, 33, guided=True),
    Region("cursors/airdrop", 132, 0, 165, 33, guided=True),
    Region("cursors/invalid", 165, 0, 198, 33, guided=True),
    Region("cursors/target", 297, 0, 330, 33, guided=True),
    Region("cursors/bombard", 363, 0, 396, 33, guided=True),
)


def to_rgba(
    sprite: Image.Image, *, keyed: bool, clear_indices: tuple[int, ...] = ()
) -> Image.Image:
    if not keyed and not clear_indices:
        return sprite.convert("RGBA")
    rgba = sprite.convert("RGBA")
    key = {TRANSPARENT_INDEX, *clear_indices} if keyed else set(clear_indices)
    alpha = sprite.point(lambda index: 0 if index in key else 255, mode="L")
    rgba.putalpha(alpha)
    return clear_transparent_rgb(rgba)


def load_pcx(path: Path) -> Image.Image:
    sheet = Image.open(path)
    if sheet.mode != "P":
        raise ValueError(f"{path} is mode {sheet.mode}, expected paletted P")
    return sheet


def save_rgba(
    image: Image.Image,
    destination: Path,
    *,
    keyed: bool,
    clear_indices: tuple[int, ...] = (),
) -> Path:
    destination.parent.mkdir(parents=True, exist_ok=True)
    to_rgba(image, keyed=keyed, clear_indices=clear_indices).save(destination)
    return destination


def convert_whole(game_dir: Path, filename: str, out_dir: Path, *, keyed: bool) -> Path:
    source = game_dir / filename
    if not source.is_file():
        raise FileNotFoundError(f"Missing UI sheet: {source}")
    sheet = load_pcx(source)
    return save_rgba(sheet, out_dir / f"{source.stem}.png", keyed=keyed)


def console_top_valley_y(
    rgba: Image.Image, *, alpha_threshold: int = 128, top_band: int = 80
) -> int:
    """Lowest y of the top silhouette (center frame under the corner towers).

    Only the top `top_band` rows are considered so interior panel holes do not count.
    """
    width, height = rgba.size
    band = min(top_band, height)
    pixels = rgba.load()
    valley = 0
    for x in range(width):
        for y in range(band):
            if pixels[x, y][3] > alpha_threshold:
                valley = max(valley, y)
                break
    return valley


def with_console_side_bars(
    rgba: Image.Image, *, cover_width: int, bar_top_y: int
) -> Image.Image:
    """Center the console on a canvas with black side pads from `bar_top_y` down.

    Pads stay transparent above `bar_top_y` (the top-silhouette valley) so the map can
    reach the same line beside the strip that it does in the center notch; black below
    that occludes gutters and clips at the window edge.
    """
    art_w, art_h = rgba.size
    total_w = max(art_w, cover_width)
    pad_left = (total_w - art_w) // 2
    canvas = Image.new("RGBA", (total_w, art_h), (0, 0, 0, 0))
    canvas.paste(rgba, (pad_left, 0), rgba)
    top = max(0, min(bar_top_y, art_h))
    if top < art_h and pad_left > 0:
        bar = Image.new("RGBA", (pad_left, art_h - top), (0, 0, 0, 255))
        canvas.paste(bar, (0, top))
        canvas.paste(bar, (pad_left + art_w, top))
    return canvas


def convert_console_sheets(
    game_dir: Path, out_dir: Path, *, keyed: bool
) -> list[Path]:
    """Write console*_nobar.png (plain) and console*.png (default, with side bars)."""
    written: list[Path] = []
    for filename in CONSOLE_SHEETS:
        source = game_dir / filename
        if not source.is_file():
            raise FileNotFoundError(f"Missing UI sheet: {source}")
        rgba = to_rgba(load_pcx(source), keyed=keyed)
        stem = source.stem
        valley = console_top_valley_y(rgba)
        print(
            f"  {stem}: top valley y={valley} "
            f"(map_overlap for style.json variants; ~{valley}/{rgba.size[1]} of height)"
        )

        nobar_path = out_dir / f"{stem}_nobar.png"
        nobar_path.parent.mkdir(parents=True, exist_ok=True)
        rgba.save(nobar_path)
        written.append(nobar_path)

        barred = with_console_side_bars(
            rgba, cover_width=CONSOLE_SIDE_COVER_WIDTH, bar_top_y=valley
        )
        barred_path = out_dir / f"{stem}.png"
        barred.save(barred_path)
        written.append(barred_path)
        pad = (barred.size[0] - rgba.size[0]) // 2
        print(
            f"  {stem}.png: {barred.size[0]}×{barred.size[1]} "
            f"(side pad {pad}px each from y={valley}; sprite_offset_x={-pad})"
        )
    return written


def crop_regions(
    sheet: Image.Image, regions: tuple[Region, ...], asset_root: Path, *, keyed: bool
) -> list[Path]:
    written: list[Path] = []
    for region in regions:
        destination = asset_root / "ui" / f"{region.path}.png"
        written.append(
            save_rgba(
                sheet.crop(region.box),
                destination,
                keyed=keyed,
                clear_indices=region.clear_indices,
            )
        )
    return written


def convert_glob(game_dir: Path, pattern: str, out_dir: Path, *, keyed: bool) -> list[Path]:
    written: list[Path] = []
    for source in sorted(game_dir.glob(pattern)):
        sheet = load_pcx(source)
        written.append(save_rgba(sheet, out_dir / f"{source.stem}.png", keyed=keyed))
    return written


def extract_ui(game_dir: Path, asset_root: Path, *, keyed: bool) -> dict[str, list[Path]]:
    if not game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {game_dir}")

    ui_root = asset_root / "ui"
    by_family: dict[str, list[Path]] = {}

    chrome: list[Path] = []
    chrome.extend(convert_console_sheets(game_dir, ui_root / "chrome", keyed=keyed))
    for filename in WHOLE_CHROME:
        chrome.append(convert_whole(game_dir, filename, ui_root / "chrome", keyed=keyed))

    for filename, regions in (
        ("console_x2.pcx", CONSOLE_X2_CROPS),
        ("console_x.pcx", CONSOLE_X_CROPS),
    ):
        sheet = load_pcx(game_dir / filename)
        if CONSOLE_X_GUIDE_INDEX not in sheet.getdata():
            raise ValueError(f"{game_dir / filename} missing guide index {CONSOLE_X_GUIDE_INDEX}")
        chrome.extend(crop_regions(sheet, regions, asset_root, keyed=keyed))

    iface = load_pcx(game_dir / "iface.pcx")
    chrome.extend(crop_regions(iface, IFACE_CROPS, asset_root, keyed=keyed))

    text = load_pcx(game_dir / "text.pcx")
    chrome.extend(crop_regions(text, TEXT_CROPS, asset_root, keyed=keyed))
    by_family["chrome"] = chrome

    cursor_sheet = load_pcx(game_dir / "Cursor.pcx")
    # Sanity: guide colour present on the sheet.
    if CURSOR_GUIDE_INDEX not in cursor_sheet.getdata():
        raise ValueError(f"{game_dir / 'Cursor.pcx'} missing guide index {CURSOR_GUIDE_INDEX}")
    by_family["cursors"] = crop_regions(cursor_sheet, CURSOR_CROPS, asset_root, keyed=keyed)

    by_family["thumbs"] = convert_glob(game_dir, "*_sm.pcx", ui_root / "thumbs", keyed=keyed)
    by_family["artboxes"] = convert_glob(game_dir, "artbox*.pcx", ui_root / "artboxes", keyed=keyed)
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

    by_family = extract_ui(args.game_dir, args.out, keyed=not args.no_transparency)
    total = sum(len(paths) for paths in by_family.values())
    print(f"ui: {total} sprites -> {args.out / 'ui'}")
    for name, paths in by_family.items():
        print(f"  {name}: {len(paths)}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, OSError) as error:
        sys.exit(str(error))
