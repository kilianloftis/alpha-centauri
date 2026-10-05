#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"

#include <vector>

namespace ac
{

// SMAC's water depth shade at a tile diamond's centre and corners: steps down the water's
// palette ramp (docs/thinker/smac-palette-lighting.md).
struct DiamondShades_t
{
    int center = 0;
    int west = 0;
    int north = 0;
    int east = 0;
    int south = 0;
};

// depthShades splits the map's floor up to ocean level into equal bands, deepest first. The
// centre takes the tile's own depth; a corner takes the average depth of the tiles that share
// it, with land at ocean level. Rows off the map are left out; x wraps. Without a map every
// vertex takes the tile's own depth.
DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap* pMap,
                                   const std::vector<int>& depthShades);

// The tint for each vertex's shade; a shade past the table's end takes its last entry.
DiamondTint_t WaterShadeTint(const DiamondShades_t& shades, const std::vector<Color_t>& tints);

} // namespace ac
