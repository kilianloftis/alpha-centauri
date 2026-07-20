#pragma once

namespace ac
{

class Graphics;
class Tile;

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Each cell shows: moisture rockiness elevation(km).
class TileRenderer
{
public:
    // Fogged tiles use a muted label color (explored memory on the world map).
    static void Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                       bool bFogged = false);
};

} // namespace ac
