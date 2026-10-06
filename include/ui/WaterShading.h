#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "graphics/Graphics.h"
#include "ui/style/UiStyle.h"

#include <string>
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

// SMAC's depth detail picks each vertex's shade from depthShades: one step per detailMeters
// below ocean level, counted back from the table's last entry, with anything deeper taking the
// first. The centre takes the tile's own depth; a corner takes the average depth of the tiles
// that share it, with land at ocean level. Rows off the map are left out; x wraps. Without a
// map every vertex takes the tile's own depth.
DiamondShades_t ResolveWaterShades(const Tile& rTile, const WorldMap* pMap,
                                   const WaterShadingStyle_t& rShading);

// SMAC's choice between deep and shelf art: the deep landform when any corner's shade reaches
// deepFromShade, the shelf landform otherwise. The centre does not count.
const std::string& SeaArtLandform(const DiamondShades_t& shades,
                                  const WaterShadingStyle_t& rShading);

// Sets each vertex's shade to its depth shade plus the range's offset, kept within 0..max.
void ApplyWaterShades(TileShape_t& rShape, const DiamondShades_t& shades,
                      const WaterShadeRange_t& range);

} // namespace ac
