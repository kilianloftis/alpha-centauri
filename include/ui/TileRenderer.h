#pragma once

#include "graphics/Graphics.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ac
{

class Tile;

// Stable variant index for a tile cell. Depends only on coords, content id, and count
// (config list order is the contract). Returns 0 when count is 0.
size_t PickSpriteIndex(int tileX, int tileY, std::string_view contentId, size_t count);

// paths[PickSpriteIndex(...)]; empty paths → empty string (caller skips the draw).
const std::string& PickSpritePath(const std::vector<std::string>& paths, int tileX, int tileY,
                                  std::string_view contentId);

// Shared terrain-tile cell drawing for the world map and location panel preview.
// Fills by forest overlay or elevation (blue water / brown land), then draws
// ResolveTileLayers bottom-to-top with scaled sprites tinted by elevation/fog.
// Fungus is an overlay sprite (not a solid fill). Missing assets fall back to
// procedural moisture/rockiness cues (or a fungus fill if that sprite is absent).
// Footprint is a 2:1 isometric diamond (width = size, height = size / 2).
// When sprite_paths lists multiple assets, PickSpritePath chooses one per tile.
class TileRenderer
{
public:
    // Fill used by the world map, location preview, and minimap (fog dims the fill).
    // Forest overrides the elevation gradient when present; fungus does not.
    static Color_t FillColor(const Tile& rTile, bool bFogged = false);

    // Fogged tiles use a dimmed fill and muted overlay colors (explored memory on the world map).
    // `size` is the diamond width in pixels; height is size / 2.
    static void Render(Graphics& rGraphics, const Tile& rTile, float x, float y, float size,
                       bool bFogged = false);
};

} // namespace ac
