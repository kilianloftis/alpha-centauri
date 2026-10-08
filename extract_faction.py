#!/usr/bin/env python3
"""
Extract individual sprites from a Sid Meier's Alpha Centauri faction .pcx file.

The original game assets are not redistributable, so this script rips them from a
local SMAC installation at build/setup time.

Usage:
    python extract_faction.py angels
    python extract_faction.py angels --game-dir "D:/Games/Alpha Centauri"
    python extract_faction.py --all
    python extract_faction.py angels --contact-sheet

Output contract (under assets/factions/<faction-id>/, gitignored):
    bases/*.png (full crops, including fortified rows for contact sheets),
    bases/overlays/{perimeter,tachyon,water_perimeter,water_tachyon}_size{N}.png
      (RGBA diffs from the bare base — identical pixels transparent),
    leaders/*.png, logos/*.png, diplomacy_landscape.png, colors.json

SMAC .pcx basenames sometimes differ from config/factions/<id>/ (gaians.pcx → gaian,
univ.pcx → university). This script maps those stems so runtime can use faction.id
as the asset directory with no hardcoded table in C++.

Base sprites use the same index-246 shadow bake as ter1.pcx objects
(extract_pcx_common.apply_shadow): land and water alphas match extract_terrain.py.

Every faction .pcx uses the identical 1024x768 layout, so the region table below
is shared by all of them.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow is required:  pip install Pillow")

from extract_pcx_common import (
    LAND_SHADOW_ALPHA,
    SEA_SHADOW_ALPHA,
    apply_shadow,
    clear_transparent_rgb,
)


# --------------------------------------------------------------------------- #
# Layout
# --------------------------------------------------------------------------- #

# Every sprite in the sheet is drawn inside a 1px guide-line box. The
# coordinates below are the *guide lines*; the sprite is the interior, so the
# extracted rect is (x0+1, y0+1, x1, y1).

# Index of the magenta key colour used as transparency across the sheet.
TRANSPARENT_INDEX = 255

# Index of the cyan guide lines that box every sprite. Only used to sanity-check
# that a .pcx really is a faction sheet.
GUIDE_INDEX = 149

# 4 columns x 6 rows of base art at the top-left.
BASE_GUIDES_X = (0, 101, 202, 303, 404)
BASE_GUIDES_Y = (0, 76, 152, 228, 304, 380, 456)

# One row of the base grid -> (subdirectory, filename stem).
BASE_ROWS = (
    "base",
    "base_defense1",
    "base_defense2",
    "water_base",
    "water_base_defense1",
    "water_base_defense2",
)

# Column -> base size stage filenames. Runtime thresholds live in config/base_sprite_sizes.json;
# SMAC sheets ship four columns, so extraction always writes size1…size4.
BASE_SIZE_STAGES = ("size1", "size2", "size3", "size4")


@dataclass(frozen=True)
class Region:
    """A single sprite, addressed by its surrounding guide-line box."""

    path: str  # output path relative to the faction root, no extension
    x0: int
    y0: int
    x1: int
    y1: int
    # When set, index-246 pixels become black at this alpha (map object shadows).
    shadow_alpha: int | None = None

    @property
    def box(self) -> tuple[int, int, int, int]:
        """PIL crop box for the interior of the guide-line box."""
        return (self.x0 + 1, self.y0 + 1, self.x1, self.y1)


def _base_regions() -> list[Region]:
    regions: list[Region] = []
    for row_index, row_name in enumerate(BASE_ROWS):
        y0 = BASE_GUIDES_Y[row_index]
        y1 = BASE_GUIDES_Y[row_index + 1]
        shadow_alpha = SEA_SHADOW_ALPHA if row_name.startswith("water_") else LAND_SHADOW_ALPHA
        for col_index, stage in enumerate(BASE_SIZE_STAGES):
            x0 = BASE_GUIDES_X[col_index]
            x1 = BASE_GUIDES_X[col_index + 1]
            regions.append(
                Region(f"bases/{row_name}_{stage}", x0, y0, x1, y1, shadow_alpha=shadow_alpha)
            )
    return regions


# 24 base images + 4 leader portraits + 8 logos + 1 diplomacy landscape.
REGIONS: tuple[Region, ...] = tuple(
    _base_regions()
    + [
        # Leader portraits.
        Region("leaders/datalink", 530, 0, 695, 199),
        Region("leaders/council_small", 87, 456, 184, 572),
        Region("leaders/council_large", 184, 456, 303, 601),
        Region("leaders/diplomacy", 303, 456, 435, 614),
        # Logos.
        Region("logos/report_primary", 700, 0, 771, 71),
        Region("logos/report_secondary", 700, 71, 771, 142),
        Region("logos/report_small_off", 771, 0, 809, 30),
        Region("logos/report_small_mouseover", 771, 30, 809, 60),
        Region("logos/report_small_on", 771, 60, 809, 90),
        Region("logos/council_primary", 0, 456, 87, 529),
        Region("logos/council_secondary", 0, 529, 87, 602),
        Region("logos/diplomacy", 435, 456, 501, 507),
        # Backdrop.
        Region("diplomacy_landscape", 501, 456, 735, 581),
    ]
)

# The seven single-colour swatches along the bottom edge. These are not really
# images: the game reads them as palette indices, so they are exported to JSON.
# Each entry is (key, guide-box top-left). All swatches are 9x9 guide boxes.
COLOR_SWATCHES = (
    ("faction_color_primary", 0, 735),
    ("faction_color_secondary", 0, 743),
    ("faction_text_color_primary", 0, 751),
    ("faction_text_color_secondary", 0, 759),
    ("border_color", 157, 745),
    ("border_alpha_percent", 157, 753),
    ("vehicle_color", 431, 740),
)
SWATCH_SIZE = 9


# --------------------------------------------------------------------------- #
# Extraction
# --------------------------------------------------------------------------- #


def load_sheet(pcx_path: Path) -> Image.Image:
    """Open the sheet, keeping it paletted so we can key out the magenta."""
    image = Image.open(pcx_path)
    if image.mode != "P":
        raise ValueError(f"{pcx_path.name}: expected a paletted PCX, got mode {image.mode!r}")
    if image.size != (1024, 768):
        raise ValueError(f"{pcx_path.name}: expected a 1024x768 sheet, got {image.size}")
    return image


def is_faction_sheet(pcx_path: Path) -> bool:
    """Cheap layout probe so --all can ignore the many non-faction .pcx files."""
    try:
        sheet = load_sheet(pcx_path)
    except (OSError, ValueError):
        return False

    pixels = sheet.load()
    # The base grid's guide-line intersections are the most distinctive marker.
    # A couple of intersections are broken by label text in the stock art, so
    # require a strong majority rather than a perfect match.
    probes = [(x, y) for x in BASE_GUIDES_X for y in BASE_GUIDES_Y]
    hits = sum(pixels[x, y] == GUIDE_INDEX for x, y in probes)
    return hits >= len(probes) * 0.9


def to_rgba(sprite: Image.Image, *, keyed: bool) -> Image.Image:
    """Convert a paletted crop to RGBA, optionally keying out the magenta."""
    if not keyed:
        return sprite.convert("RGBA")

    rgba = sprite.convert("RGBA")
    alpha = sprite.point(lambda index: 0 if index == TRANSPARENT_INDEX else 255, mode="L")
    rgba.putalpha(alpha)
    return rgba


def extract_regions(sheet: Image.Image, out_root: Path, *, keyed: bool) -> list[Path]:
    written: list[Path] = []
    for region in REGIONS:
        crop = sheet.crop(region.box)
        sprite = to_rgba(crop, keyed=keyed)
        if keyed and region.shadow_alpha is not None:
            sprite = apply_shadow(sprite, crop, region.shadow_alpha)
        if keyed:
            sprite = clear_transparent_rgb(sprite)
        destination = out_root / f"{region.path}.png"
        destination.parent.mkdir(parents=True, exist_ok=True)
        sprite.save(destination)
        written.append(destination)
    return written


def rgba_diff_overlay(bare: Image.Image, fortified: Image.Image) -> Image.Image:
    """Pixels where fortified differs from bare; transparent where RGBA matches exactly."""
    if bare.size != fortified.size:
        raise ValueError(f"diff size mismatch: {bare.size} vs {fortified.size}")
    bare_px = bare.load()
    fort_px = fortified.load()
    overlay = Image.new("RGBA", bare.size, (0, 0, 0, 0))
    out_px = overlay.load()
    width, height = bare.size
    for y in range(height):
        for x in range(width):
            if bare_px[x, y] != fort_px[x, y]:
                out_px[x, y] = fort_px[x, y]
    return overlay


# Bare row + fortified row → overlay filename stem under bases/overlays/.
_OVERLAY_DIFFS = (
    ("base", "base_defense1", "perimeter"),
    ("base", "base_defense2", "tachyon"),
    ("water_base", "water_base_defense1", "water_perimeter"),
    ("water_base", "water_base_defense2", "water_tachyon"),
)


def write_defense_overlays(out_root: Path) -> list[Path]:
    """Diff fortified base crops against bare bases into bases/overlays/."""
    written: list[Path] = []
    overlays_root = out_root / "bases" / "overlays"
    overlays_root.mkdir(parents=True, exist_ok=True)
    for bare_row, fortified_row, overlay_stem in _OVERLAY_DIFFS:
        for stage in BASE_SIZE_STAGES:
            bare_path = out_root / "bases" / f"{bare_row}_{stage}.png"
            fortified_path = out_root / "bases" / f"{fortified_row}_{stage}.png"
            bare = Image.open(bare_path).convert("RGBA")
            fortified = Image.open(fortified_path).convert("RGBA")
            destination = overlays_root / f"{overlay_stem}_{stage}.png"
            rgba_diff_overlay(bare, fortified).save(destination)
            written.append(destination)
    return written


def extract_colors(sheet: Image.Image, out_root: Path) -> Path:
    """Read the seven colour swatches and write them as JSON."""
    pixels = sheet.load()
    palette = sheet.getpalette()

    colors: dict[str, dict] = {}
    for key, x0, y0 in COLOR_SWATCHES:
        # Sample the middle of the swatch so a stray guide-line pixel can't
        # poison the reading.
        index = pixels[x0 + SWATCH_SIZE // 2, y0 + SWATCH_SIZE // 2]
        red, green, blue = palette[index * 3 : index * 3 + 3]
        colors[key] = {
            "palette_index": index,
            "rgb": [red, green, blue],
            "hex": f"#{red:02X}{green:02X}{blue:02X}",
        }

    destination = out_root / "colors.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(colors, indent=2) + "\n", encoding="utf-8")
    return destination


def write_contact_sheet(sheet: Image.Image, out_root: Path) -> Path:
    """Re-draw the sheet with every extracted region outlined, for eyeballing."""
    from PIL import ImageDraw

    proof = sheet.convert("RGB")
    draw = ImageDraw.Draw(proof)
    for region in REGIONS:
        left, top, right, bottom = region.box
        draw.rectangle([left, top, right - 1, bottom - 1], outline=(0, 255, 0))
    for _key, x0, y0 in COLOR_SWATCHES:
        draw.rectangle(
            [x0 + 1, y0 + 1, x0 + SWATCH_SIZE - 2, y0 + SWATCH_SIZE - 2],
            outline=(255, 0, 0),
        )

    destination = out_root / "_contact_sheet.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    proof.save(destination)
    return destination


def find_faction_pcx(game_dir: Path, faction: str) -> Path:
    """Locate <faction>.pcx in the install, tolerating SMAC's uppercase names."""
    if not game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {game_dir}")

    wanted = f"{faction.lower()}.pcx"
    for candidate in game_dir.iterdir():
        if candidate.is_file() and candidate.name.lower() == wanted:
            return candidate
    raise FileNotFoundError(f"{faction}.pcx not found in {game_dir}")


def extract_faction(pcx_path: Path, out_root: Path, *, keyed: bool, contact_sheet: bool) -> None:
    sheet = load_sheet(pcx_path)
    sprites = extract_regions(sheet, out_root, keyed=keyed)
    overlays = write_defense_overlays(out_root)
    extract_colors(sheet, out_root)
    if contact_sheet:
        write_contact_sheet(sheet, out_root)
    print(
        f"{pcx_path.name}: {len(sprites)} sprites + {len(overlays)} overlays + colors.json "
        f"-> {out_root}"
    )


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

# Prefer this machine's GOG/PlayOnLinux install; fall back to the stock Windows path.
_LOCAL_GAME_DIR = Path(
    "/home/martok/.PlayOnLinux/wineprefix/AlphaCentauri_gog/"
    "drive_c/GOG Games/Sid Meier's Alpha Centauri"
)
_WINDOWS_GAME_DIR = Path(r"C:\Program Files (x86)\Alpha Centauri")
DEFAULT_GAME_DIR = _LOCAL_GAME_DIR if _LOCAL_GAME_DIR.is_dir() else _WINDOWS_GAME_DIR
DEFAULT_ASSET_ROOT = Path("assets/factions")

# Stock SMAC / SMAX .pcx basenames. Some (e.g. univ.pcx) break enough guide-line pixels that
# is_faction_sheet rejects them; --all still extracts these when the file is present.
STOCK_PCX_STEMS = frozenset(
    {
        "gaians",
        "hive",
        "univ",
        "morgan",
        "spartans",
        "peace",
        "believe",
        "angels",
        "pirates",
        "drone",
        "cyborg",
        "caretake",
        "usurper",
    }
)

# SMAC .pcx stem → config/factions/<id> and assets/factions/<id>. Unlisted stems keep their name.
PCX_STEM_TO_FACTION_ID = {
    "gaians": "gaian",
    "univ": "university",
}
FACTION_ID_TO_PCX_STEM = {faction_id: pcx for pcx, faction_id in PCX_STEM_TO_FACTION_ID.items()}


def faction_id_for_pcx_stem(pcx_stem: str) -> str:
    return PCX_STEM_TO_FACTION_ID.get(pcx_stem.lower(), pcx_stem.lower())


def pcx_stem_for_faction_arg(name: str) -> str:
    """Accept either a config faction id (gaian) or a SMAC .pcx stem (gaians)."""
    lower = name.lower()
    if lower in FACTION_ID_TO_PCX_STEM:
        return FACTION_ID_TO_PCX_STEM[lower]
    return lower


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "faction",
        nargs="?",
        help="config faction id or SMAC .pcx stem, e.g. 'gaian' or 'gaians'",
    )
    parser.add_argument("--all", action="store_true", help="extract every faction found in the game directory")
    parser.add_argument("--game-dir", type=Path, default=DEFAULT_GAME_DIR, help=f"SMAC install (default: {DEFAULT_GAME_DIR})")
    parser.add_argument("--input", type=Path, help="extract this .pcx directly, ignoring --game-dir")
    parser.add_argument("--out", type=Path, default=DEFAULT_ASSET_ROOT, help=f"asset root (default: {DEFAULT_ASSET_ROOT})")
    parser.add_argument("--no-transparency", action="store_true", help="keep the magenta key colour opaque")
    parser.add_argument("--contact-sheet", action="store_true", help="also write _contact_sheet.png showing every crop")
    args = parser.parse_args(argv)

    # Each job is (output_faction_id, pcx_path).
    if args.input:
        pcx_stem = args.input.stem.lower()
        jobs = [(faction_id_for_pcx_stem(pcx_stem), args.input)]
    elif args.all:
        if not args.game_dir.is_dir():
            raise FileNotFoundError(f"Game directory not found: {args.game_dir}")
        jobs = []
        for candidate in sorted(args.game_dir.iterdir()):
            if candidate.suffix.lower() != ".pcx":
                continue
            pcx_stem = candidate.stem.lower()
            if pcx_stem in STOCK_PCX_STEMS or is_faction_sheet(candidate):
                jobs.append((faction_id_for_pcx_stem(pcx_stem), candidate))
        if not jobs:
            print(f"No faction .pcx files found in {args.game_dir}", file=sys.stderr)
            return 1
    elif args.faction:
        pcx_stem = pcx_stem_for_faction_arg(args.faction)
        jobs = [(faction_id_for_pcx_stem(pcx_stem), find_faction_pcx(args.game_dir, pcx_stem))]
    else:
        parser.error("give a faction name, --all, or --input")

    for faction_id, pcx_path in jobs:
        extract_faction(
            pcx_path,
            args.out / faction_id,
            keyed=not args.no_transparency,
            contact_sheet=args.contact_sheet,
        )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as error:
        sys.exit(str(error))
