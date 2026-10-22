#pragma once

#include "game/MapDisplayConfig.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "ui/TileShapeGeometry.h"
#include "ui/style/UiStyle.h"

namespace ac
{

using TileLifts_t = DiamondValues_t<float>;
using TileShades_t = DiamondValues_t<float>;

// A land centre lifts by elevation / levelMeters (Smooth) or whole levels (Stepped); a corner
// takes the mean of its four tiles, or sea level next to water. x wraps.
TileLifts_t ResolveTileLifts(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                             const ReliefStyle_t& rStyle);

// SMAC's slope shading: facets lighter facing the lower right, vertices averaging their facets,
// land lightening with height, all within −4…4. Water and Flat give 0.
TileShades_t ResolveTileShades(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                               const ReliefStyle_t& rStyle);

} // namespace ac
