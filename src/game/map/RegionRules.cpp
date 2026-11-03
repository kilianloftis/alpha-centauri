#include "game/map/RegionRules.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <cstddef>
#include <vector>

namespace ac
{

namespace
{

constexpr int k_Unassigned = -1;
constexpr int k_NorthLandRegion = 0;
constexpr int k_NorthWaterRegion = 1;
constexpr int k_SouthLandRegion = 2;
constexpr int k_SouthWaterRegion = 3;
constexpr int k_FirstBodyRegion = 4;

bool IsPolarRow_(const Tile& rTile, const WorldMap& rMap)
{
    return rTile.GetY() == 0 || rTile.GetY() == rMap.GetHeight() - 1;
}

// The binary gives both poles one id per surface (config/deviations/README.md "Poles are
// separate regions").
int PolarRegion_(const Tile& rTile)
{
    if (rTile.GetY() == 0)
    {
        return rTile.IsWater() ? k_NorthWaterRegion : k_NorthLandRegion;
    }
    return rTile.IsWater() ? k_SouthWaterRegion : k_SouthLandRegion;
}

// The binary's flood never expands from a polar row, so bodies never join through one.
void FloodBody_(const WorldMap& rMap, const Tile& rStart, int region, std::vector<int>& rRegions)
{
    rRegions[rMap.GetTileIndex(rStart)] = region;
    std::vector<const Tile*> frontier{&rStart};
    while (!frontier.empty())
    {
        const Tile* pTile = frontier.back();
        frontier.pop_back();
        for (const LatticeOffset_t& rStep : k_RingNeighbors)
        {
            const Tile* pNext = GetTileAtLatticeOffset(rMap, *pTile, rStep.p, rStep.q);
            if (!pNext || IsPolarRow_(*pNext, rMap) || pNext->IsWater() != rStart.IsWater())
            {
                continue;
            }
            int& rRegion = rRegions[rMap.GetTileIndex(*pNext)];
            if (rRegion == k_Unassigned)
            {
                rRegion = region;
                frontier.push_back(pNext);
            }
        }
    }
}

} // namespace

std::vector<int> AssignRegions(const WorldMap& rMap)
{
    std::vector<int> regions(rMap.GetTiles().size(), k_Unassigned);
    int nextRegion = k_FirstBodyRegion;
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const Tile& rTile = *rMap.GetTiles()[i];
        if (IsPolarRow_(rTile, rMap))
        {
            regions[i] = PolarRegion_(rTile);
        }
        else if (regions[i] == k_Unassigned)
        {
            FloodBody_(rMap, rTile, nextRegion++, regions);
        }
    }
    return regions;
}

} // namespace ac
