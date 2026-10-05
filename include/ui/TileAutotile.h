#pragma once

#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <cstdint>
#include <functional>

namespace ac
{

// Neighbor mask that picks a tile set's sprite.
// Edges: bits 0–3 are the neighbors across the diamond's NE, SE, SW, NW edges — (0, −1),
// (+1, 0), (0, +1), (−1, 0), the same order as RiverConnection_t.
// Blob: bits 0–7 run clockwise from the N corner — (−1, −1), (0, −1), (+1, −1), (+1, 0),
// (+1, +1), (0, +1), (−1, +1), (−1, 0) — and a corner bit stays only when both edge bits beside
// it are set, leaving 47 masks.
// Rows off the map never match; x wraps.
std::uint8_t ResolveTileMask(SpriteTileLayout_t layout, const Tile& rTile, const WorldMap& rMap,
                             const std::function<bool(const Tile& rNeighbor)>& matches);

} // namespace ac
