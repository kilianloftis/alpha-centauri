#pragma once

#include "graphics/Graphics.h"

namespace ac
{

class Tile;

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Fills by elevation (blue water / brown land), then overlays moisture rockiness elevation(km).
class TileRenderer
{
public:
    // Elevation fill used by the world map, location preview, and minimap (fog dims the fill).
    static Color_t FillColor(const Tile& rTile, bool bFogged = false);

    // Fogged tiles use a dimmed fill and muted label color (explored memory on the world map).
    static void Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                       bool bFogged = false);
};

} // namespace ac
