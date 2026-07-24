#include "game/map/RiverGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <unordered_set>
#include <vector>

namespace ac
{

bool TileTerminatesRiver(const Tile& rTile)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature && pFeature->terminatesRiver)
        {
            return true;
        }
    }
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement && pImprovement->terminatesRiver)
        {
            return true;
        }
    }
    return false;
}

void TraceRiverFrom(Tile& rOrigin, WorldMap& rWorld)
{
    std::unordered_set<int> visited;
    Tile* pCurrent = &rOrigin;

    while (pCurrent)
    {
        const int index = rWorld.GetTileIndex(*pCurrent);
        if (!visited.insert(index).second)
        {
            break;
        }

        pCurrent->SetHasRiver(true);

        if (pCurrent->IsWater() || TileTerminatesRiver(*pCurrent))
        {
            break;
        }

        Tile* pBest = nullptr;
        int bestElev = 0;
        ForEachOrthogonalNeighbor(*pCurrent, rWorld, [&](Tile* pNeighbor)
        {
            if (!pBest || pNeighbor->GetElevation() < bestElev)
            {
                pBest = pNeighbor;
                bestElev = pNeighbor->GetElevation();
            }
        });

        if (!pBest || bestElev >= pCurrent->GetElevation())
        {
            break;
        }

        pCurrent = pBest;
    }
}

void RecomputeRivers(WorldMap& rWorld)
{
    std::vector<Tile*> aquifers;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        pTile->SetHasRiver(false);
        if (pTile->GetHasAquifer())
        {
            aquifers.push_back(pTile.get());
        }
    }

    for (Tile* pAquifer : aquifers)
    {
        TraceRiverFrom(*pAquifer, rWorld);
    }
}

} // namespace ac
