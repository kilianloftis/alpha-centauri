#pragma once

#include "graphics/Graphics.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class Tile;
class WorldMap;

// Stable variant index for a tile cell. Depends only on coords, content id, and count
// (config list order is the contract). Returns 0 when count is 0.
size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count);

// paths[PickSpriteIndex(...)]; empty paths → empty string (caller skips the draw).
const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId);

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Fills by forest overlay or elevation (blue water / brown land), then draws
// ResolveTileLayers bottom-to-top. Land art keeps its sheet colours; water art is shaded per
// vertex by depth, as SMAC does. Fungus is an overlay sprite (not a solid fill). Missing
// assets fall back to procedural moisture/rockiness cues (or a fungus fill if that sprite is
// absent). Footprint is a 2:1 isometric diamond (width = size, height = size / 2).
// When sprite_paths lists multiple assets, PickSpritePath chooses one per tile.
class TileRenderer
{
public:
    // Fill used by the world map, location preview, and minimap (fog dims the fill).
    // Forest overrides the elevation gradient when present; fungus does not.
    static Color_t FillColor(const Tile& rTile, bool bFogged = false);

    // Fogged tiles (explored memory on the world map) dim their land art and get a haze over
    // the terrain layers; object sprites stay clear on top.
    // `size` is the diamond width in pixels; height is size / 2.
    // pMap enables neighbor-aware tile sets, rockiness insets, coast overlays and water corner
    // shading; null draws no coast and shades water from the tile alone (location preview).
    static void Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                       bool bFogged = false, const WorldMap* pMap = nullptr);
};

} // namespace ac
