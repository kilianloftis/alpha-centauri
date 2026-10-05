#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace ac
{

// Diamond corners in screen space. Each owns the quarter of the tile around it.
enum class CoastCorner_t
{
    West,
    North,
    East,
    South,
};

inline constexpr std::size_t k_CoastCornerCount = 4;

struct CoastCornerArt_t
{
    CoastCorner_t corner = CoastCorner_t::West;
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
    std::vector<std::reference_wrapper<const Tile>> waterNeighbors;
};

// Water tiles get no coast. Rows off the map count as land; x wraps.
CoastOverlay_t ResolveCoastOverlay(const Tile& rTile, const WorldMap& rMap);

} // namespace ac
