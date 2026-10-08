#include "ui/CoastOverlay.h"

#include "game/map/MapUtils.h"

namespace ac
{

namespace
{

constexpr std::uint8_t k_AllWater = 7;

bool IsWaterNeighbor_(const Tile& rTile, const WorldMap& rMap, const LatticeOffset_t& offset)
{
    const Tile* pNeighbor = GetTileAtLatticeOffset(rMap, rTile, offset.p, offset.q);
    return pNeighbor && pNeighbor->IsWater();
}

} // namespace

CoastOverlay_t ResolveCoastOverlay(const Tile& rTile, const WorldMap& rMap)
{
    CoastOverlay_t overlay;
    for (std::size_t i = 0; i < k_CoastCornerCount; ++i)
    {
        overlay.corners[i].corner = static_cast<DiamondCorner_t>(i);
    }
    if (rTile.IsWater())
    {
        return overlay;
    }

    const bool bOddRow = (rTile.GetY() & 1) != 0;
    for (CoastCornerArt_t& rArt : overlay.corners)
    {
        std::uint8_t bit = 1;
        for (const LatticeOffset_t& offset :
             k_CornerNeighbors[static_cast<std::size_t>(rArt.corner)])
        {
            if (IsWaterNeighbor_(rTile, rMap, offset))
            {
                rArt.waterMask = static_cast<std::uint8_t>(rArt.waterMask | bit);
            }
            bit = static_cast<std::uint8_t>(bit << 1);
        }
        rArt.bAlternate = bOddRow && rArt.waterMask == k_AllWater;
    }
    return overlay;
}

} // namespace ac
