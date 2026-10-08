"""Shared PCX object-sprite helpers for extract_terrain.py / extract_faction.py.

SMAC marks object shadows with palette index 246 (peach). After the loader's +10 slot
shift that wraps to 0, Sprite_draw_dest darkens the terrain under those pixels through
the shadow table (shadow.tmp). We bake the same look as partly transparent black so
RGBA drawing darkens whatever sits underneath.
"""

from __future__ import annotations

from PIL import Image

# SMAC object-shadow marker (ter1.pcx and faction base sheets).
SHADOW_INDEX = 246
# Black at these alphas matches the shadow table on average: land → 0.79 luminance,
# water → 0.92.
LAND_SHADOW_ALPHA = 54
SEA_SHADOW_ALPHA = 21


def apply_shadow(rgba: Image.Image, sprite: Image.Image, alpha: int) -> Image.Image:
    """Turn shadow-index pixels into black at the given alpha."""
    mask = sprite.point(lambda index: 255 if index == SHADOW_INDEX else 0, mode="L")
    rgba.paste(Image.new("RGBA", rgba.size, (0, 0, 0, alpha)), (0, 0), mask)
    return rgba


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
