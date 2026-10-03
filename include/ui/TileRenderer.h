#pragma once

#include "graphics/Graphics.h"

namespace ac
{

class Tile;

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Fills by fungus/forest overlay or elevation (blue water / brown land), then draws
// ResolveTileLayers bottom-to-top with scaled sprites tinted by elevation/fog.
// Missing assets fall back to procedural moisture/rockiness cues.
class TileRenderer
{
public:
    // Fill used by the world map, location preview, and minimap (fog dims the fill).
    // Fungus and Forest override the elevation gradient when present.
    static Color_t FillColor(const Tile& rTile, bool bFogged = false);

    // Fogged tiles use a dimmed fill and muted overlay colors (explored memory on the world map).
    static void Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                       bool bFogged = false);
};

} // namespace ac
