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

Every texture.pcx cell is baked onto a 112×56 diamond the way terranx.exe maps it: the 56×56
square is turned 45° so its corners land on the diamond's corners (orientation table in
docs/thinker/smac-terrain-textures.md). Neighboring diamonds partition the plane exactly.

Rainfall-grid layout (col, row) — left 4×N block:
  (0,0)/(2,0) rolling overlays; (1,0)/(3,0) rocky overlays; (0,1) arid base;
  rows 2–5 moist, rows 6–9 wet blend cells. Flat has no dedicated art.

Tile sets, one sprite per neighbor mask, in sprites/landforms/<set>/<mask>.png:
  blob sets (moist, wet, fungus_land, fungus_sea, jungle): 47 masks over all eight
  neighbors, clockwise from the N corner; a corner bit counts only when both edges beside
  it are set. Each mask is SMAC's blend shape at its quarter-turn rotation.
  edge sets (forest, river): 16 masks over the NE/SE/SW/NW edge neighbors (bits 0–3).

ter1.pcx object sprites are 100×62: a 100×50 footprint diamond with 12 px of art above it.
Purple 253 is the key and dark-purple 252 marks the footprint; SMAC drops both. Tile
bonuses come in two sea and two land variants per resource.

Coastlines are baked from Rainfall.pcx (see docs/thinker/smac-coastline-rainfall.md):
sprites/coast/{water,shore}_<corner>_<case>.png, one pair per diamond corner (w/n/e/s)
and water case (1–7, plus 7_alt). Each is a tile-sized overlay for a land tile: water
pixels carry the ocean shelf texture, shore pixels the Rainfall.pcx shore colours.
"""

from __future__ import annotations

import argparse
import functools
import math
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

# texture.pcx transparency (both indices are empty on the sheet).
TEXTURE_KEY_INDICES = frozenset({0, 255})
# ter1.pcx purple key 253 and footprint guide 252 (+ spare indices sometimes present).
TER1_KEY_INDICES = frozenset({0, 252, 253, 255})
TER1_SPRITE_WIDTH = 100
TER1_SPRITE_HEIGHT = 62

# Ocean cells right of the rainfall grid, between guide rows at y=78/135/192.
OCEAN_SHELF_BOX = (280, 79, 280 + TEXTURE_CELL, 79 + TEXTURE_CELL)
OCEAN_DEEP_BOX = (280, 136, 280 + TEXTURE_CELL, 136 + TEXTURE_CELL)

RAINFALL_SIZE = (640, 480)

TILE_SPRITE_SIZE = (112, 56)

# Cell corners (u, v) on the 56-unit square.
TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT = (0, 0), (56, 0), (56, 56), (0, 56)
# Cell corner at each screen corner W, N, E, S (terranx.exe UV tables 0x686090 / 0x686098 and
# the fixed forest/river quads). Blend rotation r gives corner k the entry (k + r) & 3.
BLEND_CORNERS = (BOTTOM_LEFT, TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT)
EDGE_CORNERS = (TOP_LEFT, TOP_RIGHT, BOTTOM_RIGHT, BOTTOM_LEFT)

# Reduced blob mask → (shape, rotation) from terranx.exe's blend table at 0x685484.
BLOB_SHAPES: dict[int, tuple[int, int]] = {
    0: (0, 0), 2: (1, 0), 8: (1, 3), 10: (2, 0), 14: (3, 0), 32: (1, 2), 34: (4, 0),
    40: (2, 3), 42: (5, 0), 46: (6, 0), 56: (3, 3), 58: (7, 0), 62: (8, 0), 128: (1, 1),
    130: (2, 1), 131: (3, 1), 136: (4, 3), 138: (5, 1), 139: (6, 1), 142: (7, 1), 143: (8, 1),
    160: (2, 2), 162: (5, 2), 163: (7, 2), 168: (5, 3), 170: (9, 0), 171: (10, 0),
    174: (10, 3), 175: (11, 0), 184: (6, 3), 186: (10, 2), 187: (12, 0), 190: (11, 3),
    191: (13, 0), 224: (3, 2), 226: (6, 2), 227: (8, 2), 232: (7, 3), 234: (10, 1),
    235: (11, 1), 238: (12, 3), 239: (13, 1), 248: (8, 3), 250: (11, 2), 251: (13, 2),
    254: (13, 3), 255: (14, 0),
}
EDGE_MASKS = tuple(range(16))

# --------------------------------------------------------------------------- #
# Coastlines (terranx.exe coast pass)
# --------------------------------------------------------------------------- #

# 56×56 template; each diamond corner samples one quarter of it.
COAST_TEMPLATE_ORIGIN = (2, 333)
COAST_TEMPLATE_SIZE = 56
# Codes 2–5 draw the colour found at these Rainfall.pcx pixels.
COAST_CODE_COLOR_PIXELS = {2: (130, 361), 3: (144, 361), 4: (158, 361), 5: (172, 361)}

# Template placeholder index → code per water mask 0..7 (terranx.exe 0x6846F8 / 0x684730).
# 0 keeps the land pixel, 1 draws ocean, 2–5 draw a shore colour.
COAST_CODES: dict[int, str] = {
    5: "00000000", 148: "01010101", 67: "01010111", 131: "00001111", 180: "00011111",
    201: "00000001", 7: "05050505", 9: "04040404", 11: "03030303", 13: "02010202",
    39: "00005555", 41: "00004444", 43: "00003333", 45: "00002222", 193: "00000005",
    194: "00000004", 195: "00000003", 196: "00000002", 58: "05050501", 59: "04040401",
    60: "03030301", 62: "02020201", 146: "00005551", 147: "00004441", 149: "00003331",
    151: "00002221", 23: "00050051", 25: "00040041", 27: "00030031", 29: "00020021",
    71: "01010151", 73: "01010141", 75: "01010131", 77: "01010121", 103: "00051111",
    105: "00041111", 107: "00031111", 109: "00021111", 136: "05010511", 137: "04010411",
    139: "03010311", 140: "02010211", 116: "00015511", 118: "00014411", 119: "00013311",
    121: "00012211", 88: "05012011", 89: "04013011", 90: "03014011", 165: "05515011",
    166: "04414011", 96: "02010311", 144: "00015311",
}
# Second all-water shape (mask 7, odd rows; terranx.exe 0x4653AC): these placeholders keep
# land, and each ramp takes codes 5, 4, 3, 2 in order.
COAST_ALT_LAND = frozenset({201, 193, 194, 195, 196})
COAST_ALT_RAMPS = ((58, 59, 60, 62), (146, 147, 149, 151), (23, 25, 27, 29))

COAST_SPRITE_SIZE = TILE_SPRITE_SIZE
COAST_CORNERS = ("w", "n", "e", "s")
# Template quarter per corner as (u half, v half): square top-left is the W corner.
COAST_CORNER_QUARTERS = {"w": (0, 0), "n": (1, 0), "e": (1, 1), "s": (0, 1)}
COAST_CASES = ("1", "2", "3", "4", "5", "6", "7", "7_alt")


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
    # Texture cells are baked onto the diamond; ter1 objects keep their own canvas.
    diamond: bool = True

    @property
    def box(self) -> tuple[int, int, int, int]:
        return (self.x0, self.y0, self.x1, self.y1)


def _grid_cell(col: int, row: int) -> tuple[int, int, int, int]:
    x0 = 1 + col * TEXTURE_STRIDE
    y0 = 1 + row * TEXTURE_STRIDE
    return (x0, y0, x0 + TEXTURE_CELL, y0 + TEXTURE_CELL)


@dataclass(frozen=True)
class TileSet:
    """A grid of cells on texture.pcx drawn one per neighbor mask."""

    name: str  # directory under sprites/landforms
    x0: int
    y0: int
    cells: int
    blob: bool  # blob: 8-neighbor blend shapes with rotation; else 4-edge cells

    def cell_box(self, index: int) -> tuple[int, int, int, int]:
        x = self.x0 + (index % 4) * TEXTURE_STRIDE
        y = self.y0 + (index // 4) * TEXTURE_STRIDE
        return (x, y, x + TEXTURE_CELL, y + TEXTURE_CELL)

    def masks(self) -> tuple[int, ...]:
        return tuple(BLOB_SHAPES) if self.blob else EDGE_MASKS


TILE_SETS: tuple[TileSet, ...] = (
    TileSet("moist", 1, 115, 16, blob=True),
    TileSet("wet", 1, 343, 16, blob=True),
    TileSet("fungus_land", 280, 516, 15, blob=True),
    TileSet("fungus_sea", 508, 516, 15, blob=True),
    TileSet("jungle", 526, 259, 15, blob=True),
    TileSet("forest", 526, 6, 16, blob=False),
    TileSet("river", 280, 259, 16, blob=False),
)


def _texture_regions() -> list[Region]:
    return [
        Region("sprites/landforms/arid", *_grid_cell(0, 1)),
        # Rockiness overlays on row 0 (magenta-keyed, not full bases).
        Region("sprites/landforms/rolling_0", *_grid_cell(0, 0)),
        Region("sprites/landforms/rolling_1", *_grid_cell(2, 0)),
        Region("sprites/landforms/rocky_0", *_grid_cell(1, 0)),
        Region("sprites/landforms/rocky_1", *_grid_cell(3, 0)),
        Region("sprites/landforms/water", *OCEAN_SHELF_BOX),
        Region("sprites/landforms/ocean_shelf", *OCEAN_SHELF_BOX),
        Region("sprites/landforms/ocean", *OCEAN_DEEP_BOX),
    ]


def _ter1_object(path: str, x0: int, y0: int) -> Region:
    return Region(
        path,
        x0,
        y0,
        x0 + TER1_SPRITE_WIDTH,
        y0 + TER1_SPRITE_HEIGHT,
        sheet="ter1",
        key_indices=TER1_KEY_INDICES,
        diamond=False,
    )


def _ter1_regions() -> list[Region]:
    regions = [_ter1_object("sprites/tile_bonuses/monolith", 304, 1)]
    # Resource grid (guides every 63 px from y=252, 101 px from x=0): nutrients, minerals,
    # energy rows; columns are sea, sea, land, land.
    for row, resource in enumerate(("nutrients", "minerals", "energy")):
        for column, surface in enumerate(("sea", "sea", "land", "land")):
            regions.append(
                _ter1_object(
                    f"sprites/tile_bonuses/{resource}_{surface}_{column % 2}",
                    1 + column * 101,
                    253 + row * 63,
                )
            )
    return regions


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


@functools.lru_cache(maxsize=None)
def _diamond_texels(
    corners: tuple[tuple[int, int], ...],
) -> tuple[tuple[int, int, int, int], ...]:
    """(x, y, u, v) for each diamond pixel; `corners` are the cell corners at W, N, E, S.

    s runs from the W corner toward N and t from W toward S; the half-open test makes
    neighboring diamonds share no pixel and leave none uncovered.
    """
    width, height = TILE_SPRITE_SIZE
    (west_u, west_v), (north_u, north_v), _, (south_u, south_v) = corners
    last = TEXTURE_CELL - 1
    texels: list[tuple[int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            a = (x + 0.5) / width
            b = (y + 0.5) / height - 0.5
            s = a - b
            t = a + b
            if not (0.0 <= s < 1.0 and 0.0 <= t < 1.0):
                continue
            u = west_u + s * (north_u - west_u) + t * (south_u - west_u)
            v = west_v + s * (north_v - west_v) + t * (south_v - west_v)
            texels.append(
                (x, y, min(max(math.floor(u), 0), last), min(max(math.floor(v), 0), last))
            )
    return tuple(texels)


def bake_diamond(
    cell: Image.Image,
    corners: tuple[tuple[int, int], ...],
    key_indices: frozenset[int],
    *,
    keyed: bool,
) -> Image.Image:
    """Map a 56×56 paletted cell onto the tile diamond."""
    palette = cell.getpalette()
    if not palette or len(palette) < 256 * 3:
        raise ValueError("expected a 256-colour palette")
    source = cell.load()
    sprite = Image.new("RGBA", TILE_SPRITE_SIZE, (0, 0, 0, 0))
    pixels = sprite.load()
    for x, y, u, v in _diamond_texels(corners):
        index = source[u, v]
        if keyed and index in key_indices:
            continue
        pixels[x, y] = (palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2], 255)
    return sprite


def blend_corners(rotation: int) -> tuple[tuple[int, int], ...]:
    return tuple(BLEND_CORNERS[(k + rotation) & 3] for k in range(4))


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


def extract_regions(
    sheets: dict[str, Image.Image],
    out_root: Path,
    *,
    keyed: bool,
) -> list[Path]:
    written: list[Path] = []
    for region in REGIONS:
        crop = sheets[region.sheet].crop(region.box)
        if region.diamond:
            sprite = bake_diamond(crop, blend_corners(0), region.key_indices, keyed=keyed)
        else:
            sprite = clear_transparent_rgb(to_rgba(crop, region.key_indices, keyed=keyed))
        destination = out_root / f"{region.path}.png"
        destination.parent.mkdir(parents=True, exist_ok=True)
        sprite.save(destination)
        written.append(destination)
    return written


def bake_tile_sets(texture: Image.Image, out_root: Path, *, keyed: bool) -> list[Path]:
    """Write sprites/landforms/<set>/<mask>.png for every tile set and mask."""
    written: list[Path] = []
    for tile_set in TILE_SETS:
        out_dir = out_root / "sprites/landforms" / tile_set.name
        out_dir.mkdir(parents=True, exist_ok=True)
        for mask in tile_set.masks():
            if tile_set.blob:
                shape, rotation = BLOB_SHAPES[mask]
                corners = blend_corners(rotation)
            else:
                shape, corners = mask, EDGE_CORNERS
            cell = texture.crop(tile_set.cell_box(shape))
            sprite = bake_diamond(cell, corners, TEXTURE_KEY_INDICES, keyed=keyed)
            destination = out_dir / f"{mask}.png"
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
    for tile_set in TILE_SETS:
        for index in range(tile_set.cells):
            left, top, right, bottom = tile_set.cell_box(index)
            draw.rectangle([left, top, right - 1, bottom - 1], outline=(0, 255, 255))
            draw.text((left + 2, top + 2), f"{tile_set.name} {index}", fill=(0, 255, 255))
    destination = out_root / "sprites/_terrain_contact_sheet.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    texture.save(destination)
    return destination


def coast_code(index: int, mask: int, alternate: bool) -> int:
    """Code for one template placeholder under a corner's 3-bit water mask."""
    if alternate:
        if index in COAST_ALT_LAND:
            return 0
        for ramp in COAST_ALT_RAMPS:
            if index in ramp:
                return 5 - ramp.index(index)
    codes = COAST_CODES.get(index)
    return int(codes[mask]) if codes else 0


def _coast_corner_texels(corner: str) -> list[tuple[int, int, int, int]]:
    """(x, y, u, v) for each overlay pixel inside the corner's template quarter.

    The template square sits on the diamond rotated 45°: s runs from the W corner toward N,
    t from W toward S. Quarter sides on the tile's outer edge extend one texel outward so the
    overlay covers the jagged edge of the land sprite's diamond mask.
    """
    width, height = COAST_SPRITE_SIZE
    half_u, half_v = COAST_CORNER_QUARTERS[corner]
    bleed = 1.0 / COAST_TEMPLATE_SIZE
    s_low, s_high = (-bleed, 0.5) if half_u == 0 else (0.5, 1.0 + bleed)
    t_low, t_high = (-bleed, 0.5) if half_v == 0 else (0.5, 1.0 + bleed)
    last = COAST_TEMPLATE_SIZE - 1
    texels: list[tuple[int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            a = (x + 0.5) / width
            b = (y + 0.5) / height - 0.5
            s = a - b
            t = a + b
            if not (s_low <= s < s_high and t_low <= t < t_high):
                continue
            u = min(max(math.floor(s * COAST_TEMPLATE_SIZE), 0), last)
            v = min(max(math.floor(t * COAST_TEMPLATE_SIZE), 0), last)
            texels.append((x, y, u, v))
    return texels


def bake_coast_sprites(texture: Image.Image, rainfall: Image.Image, out_root: Path) -> list[Path]:
    """Write the water and shore overlays for every corner and water case."""
    palette = texture.getpalette()
    if not palette or len(palette) < 256 * 3:
        raise ValueError("texture.pcx: expected a 256-colour palette")

    def rgb(index: int) -> tuple[int, int, int]:
        return (palette[index * 3], palette[index * 3 + 1], palette[index * 3 + 2])

    left, top = COAST_TEMPLATE_ORIGIN
    template = rainfall.crop(
        (left, top, left + COAST_TEMPLATE_SIZE, top + COAST_TEMPLATE_SIZE)
    ).load()
    shelf = texture.crop(OCEAN_SHELF_BOX).load()
    code_colors = {
        code: rgb(rainfall.getpixel(pixel)) for code, pixel in COAST_CODE_COLOR_PIXELS.items()
    }

    out_dir = out_root / "sprites/coast"
    out_dir.mkdir(parents=True, exist_ok=True)
    written: list[Path] = []
    for corner in COAST_CORNERS:
        texels = _coast_corner_texels(corner)
        for case in COAST_CASES:
            mask = int(case[0])
            alternate = case.endswith("_alt")
            water = Image.new("RGBA", COAST_SPRITE_SIZE, (0, 0, 0, 0))
            shore = Image.new("RGBA", COAST_SPRITE_SIZE, (0, 0, 0, 0))
            water_pixels = water.load()
            shore_pixels = shore.load()
            for x, y, u, v in texels:
                code = coast_code(template[u, v], mask, alternate)
                if code == 1:
                    water_pixels[x, y] = (*rgb(shelf[u, v]), 255)
                elif code >= 2:
                    shore_pixels[x, y] = (*code_colors[code], 255)
            for part, sprite in (("water", water), ("shore", shore)):
                destination = out_dir / f"{part}_{corner}_{case}.png"
                sprite.save(destination)
                written.append(destination)
    return written


def write_coast_contact_sheet(out_root: Path) -> Path:
    """One row per corner, one column per case: water + shore over a flat land diamond."""
    width, height = COAST_SPRITE_SIZE
    gap = 8
    left_margin = 24
    top_margin = 16
    sheet = Image.new(
        "RGBA",
        (
            left_margin + len(COAST_CASES) * (width + gap),
            top_margin + len(COAST_CORNERS) * (height + gap),
        ),
        (24, 24, 24, 255),
    )
    draw = ImageDraw.Draw(sheet)
    land = Image.new("RGBA", COAST_SPRITE_SIZE, (0, 0, 0, 0))
    ImageDraw.Draw(land).polygon(
        [(width // 2, 0), (width - 1, height // 2), (width // 2, height - 1), (0, height // 2)],
        fill=(96, 120, 64, 255),
    )
    coast_dir = out_root / "sprites/coast"
    for column, case in enumerate(COAST_CASES):
        draw.text((left_margin + column * (width + gap) + 2, 2), case, fill=(255, 255, 255))
    for row, corner in enumerate(COAST_CORNERS):
        y = top_margin + row * (height + gap)
        draw.text((4, y + height // 2 - 6), corner, fill=(255, 255, 255))
        for column, case in enumerate(COAST_CASES):
            x = left_margin + column * (width + gap)
            sheet.alpha_composite(land, (x, y))
            for part in ("water", "shore"):
                with Image.open(coast_dir / f"{part}_{corner}_{case}.png") as overlay:
                    sheet.alpha_composite(overlay.convert("RGBA"), (x, y))
    destination = out_root / "sprites/_coast_contact_sheet.png"
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).convert("RGB").save(
        destination
    )
    return destination


def write_tiles_contact_sheet(out_root: Path) -> Path:
    """One row per tile set, sprites in mask order, each labelled with its mask."""
    width, height = TILE_SPRITE_SIZE
    gap = 4
    label = 12
    left_margin = 90
    columns = max(len(tile_set.masks()) for tile_set in TILE_SETS)
    sheet = Image.new(
        "RGBA",
        (left_margin + columns * (width + gap), len(TILE_SETS) * (height + label + gap)),
        (24, 24, 24, 255),
    )
    draw = ImageDraw.Draw(sheet)
    for row, tile_set in enumerate(TILE_SETS):
        y = row * (height + label + gap)
        draw.text((4, y + label + height // 2 - 6), tile_set.name, fill=(255, 255, 255))
        for column, mask in enumerate(tile_set.masks()):
            x = left_margin + column * (width + gap)
            draw.text((x + 2, y), str(mask), fill=(200, 200, 200))
            path = out_root / "sprites/landforms" / tile_set.name / f"{mask}.png"
            with Image.open(path) as sprite:
                sheet.alpha_composite(sprite.convert("RGBA"), (x, y + label))
    destination = out_root / "sprites/_tiles_contact_sheet.png"
    sheet.convert("RGB").save(destination)
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
        help="also write sprites/_terrain_contact_sheet.png outlining texture crops, "
        "sprites/_tiles_contact_sheet.png showing every tile set and "
        "sprites/_coast_contact_sheet.png showing every coast overlay",
    )
    args = parser.parse_args(argv)

    if not args.game_dir.is_dir():
        raise FileNotFoundError(f"Game directory not found: {args.game_dir}")

    sheets = {
        "texture": load_sheet(find_pcx(args.game_dir, "texture")),
        "ter1": load_sheet(find_pcx(args.game_dir, "ter1")),
        "rainfall": load_sheet(find_pcx(args.game_dir, "rainfall"), RAINFALL_SIZE),
    }
    keyed = not args.no_transparency
    written = extract_regions(sheets, args.out, keyed=keyed)
    written += bake_tile_sets(sheets["texture"], args.out, keyed=keyed)
    written += bake_coast_sprites(sheets["texture"], sheets["rainfall"], args.out)
    if args.contact_sheet:
        write_contact_sheet(sheets, args.out)
        write_tiles_contact_sheet(args.out)
        write_coast_contact_sheet(args.out)
    print(f"terrain: {len(written)} sprites -> {args.out}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError) as error:
        sys.exit(str(error))
