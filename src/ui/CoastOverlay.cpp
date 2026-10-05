#include "ui/CoastOverlay.h"

namespace ac
{

namespace
{

struct GridDelta_t
{
    int dx = 0;
    int dy = 0;
};

constexpr std::uint8_t k_AllWater = 7;

// Neighbors behind mask bits 1, 2 and 4 of each corner. On the iso projection (screenX ∝ x − y,
// screenY ∝ x + y) orthogonal neighbors share a diamond edge and diagonal ones touch a corner.
constexpr std::array<std::array<GridDelta_t, 3>, k_CoastCornerCount> k_CornerNeighbors = {{
    {{{0, 1}, {-1, 1}, {-1, 0}}},   // West: SW edge, W corner, NW edge
    {{{-1, 0}, {-1, -1}, {0, -1}}}, // North: NW edge, N corner, NE edge
    {{{0, -1}, {1, -1}, {1, 0}}},   // East: NE edge, E corner, SE edge
    {{{1, 0}, {1, 1}, {0, 1}}},     // South: SE edge, S corner, SW edge
}};

constexpr std::array<GridDelta_t, 8> k_Neighbors = {{
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
}};

const Tile* WaterNeighbor_(const Tile& rTile, const WorldMap& rMap, const GridDelta_t& delta)
{
    const Tile* pNeighbor = rMap.GetTile(rTile.GetX() + delta.dx, rTile.GetY() + delta.dy);
    return pNeighbor && pNeighbor->IsWater() ? pNeighbor : nullptr;
}

} // namespace

CoastOverlay_t ResolveCoastOverlay(const Tile& rTile, const WorldMap& rMap)
{
    CoastOverlay_t overlay;
    for (std::size_t i = 0; i < k_CoastCornerCount; ++i)
    {
        overlay.corners[i].corner = static_cast<CoastCorner_t>(i);
    }
    if (rTile.IsWater())
    {
        return overlay;
    }

    const bool bOddRow = (rTile.GetY() & 1) != 0;
    for (CoastCornerArt_t& rArt : overlay.corners)
    {
        std::uint8_t bit = 1;
        for (const GridDelta_t& delta : k_CornerNeighbors[static_cast<std::size_t>(rArt.corner)])
        {
            if (WaterNeighbor_(rTile, rMap, delta))
            {
                rArt.waterMask = static_cast<std::uint8_t>(rArt.waterMask | bit);
            }
            bit = static_cast<std::uint8_t>(bit << 1);
        }
        rArt.bAlternate = bOddRow && rArt.waterMask == k_AllWater;
    }

    for (const GridDelta_t& delta : k_Neighbors)
    {
        if (const Tile* pWater = WaterNeighbor_(rTile, rMap, delta))
        {
            overlay.waterNeighbors.emplace_back(*pWater);
        }
    }
    return overlay;
}

} // namespace ac
