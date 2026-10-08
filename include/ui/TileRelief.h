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

// A land centre lifts elevation / levelMeters levels (Smooth) or whole levels (Stepped); Flat
// lifts nothing and water does not lift. A corner takes the mean of the four tiles that share
// it, or stays at sea level when one of them is water or off the map. x wraps.
TileLifts_t ResolveTileLifts(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                             const ReliefStyle_t& rStyle);

// The largest lift any tile on a map with these rules can have, in tile widths.
float MaxTileLift(const ElevationRulesConfig_t& rRules, ReliefMode_t mode,
                  const ReliefStyle_t& rStyle);

// SMAC's slope shading. Each facet (the centre and two neighboring corners) is shaded by the
// direction it faces, lighter toward the screen's lower right, at full strength once a corner
// rises the style's full-shade rise; a vertex averages the facets that meet at it. Land also
// lightens with its height above sea level, and every vertex stays within SMAC's −4…4. Water and
// Flat mode give 0.
TileShades_t ResolveTileShades(const Tile& rTile, const WorldMap& rMap, ReliefMode_t mode,
                               const ReliefStyle_t& rStyle);

} // namespace ac
