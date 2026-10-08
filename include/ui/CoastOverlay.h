#pragma once

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace ac
{

inline constexpr std::size_t k_CoastCornerCount = k_DiamondCornerCount;

struct CoastCornerArt_t
{
    DiamondCorner_t corner = DiamondCorner_t::West;
    // 1 = edge neighbor counter-clockwise of the corner, 2 = corner neighbor, 4 = edge neighbor
    // clockwise of it. 0 = no coast in this quarter.
    std::uint8_t waterMask = 0;
    // SMAC's second all-water shape: waterMask 7 on odd rows.
    bool bAlternate = false;
};

// SMAC's Rainfall.pcx coast pass for one tile (docs/thinker/smac-coastline-rainfall.md).
struct CoastOverlay_t
{
    std::array<CoastCornerArt_t, k_CoastCornerCount> corners{};
};

// Water tiles get no coast. Rows off the map count as land; x wraps.
CoastOverlay_t ResolveCoastOverlay(const Tile& rTile, const WorldMap& rMap);

} // namespace ac
