#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <cstdint>
#include <functional>

namespace ac
{

// The neighbour mask that picks a tile set's sprite: edges NE, SE, SW, NW in bits 0–3, or blob
// corners clockwise from N (47 masks). Rows off the map never match; x wraps.
std::uint8_t ResolveTileMask(SpriteTileLayout_t layout, const Tile& rTile, const WorldMap& rMap,
                             const std::function<bool(const Tile& rNeighbor)>& matches);

} // namespace ac
