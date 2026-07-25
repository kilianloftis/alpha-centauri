#include "game/map/RiverGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <unordered_set>
#include <vector>

namespace ac
{

RiverConnection_t GetRiverConnections(const Tile& rTile, const WorldMap& rWorld)
{
    if (!rTile.GetHasRiver())
    {
        return RiverConnection_t::None;
    }

    // Parallel to ForEachOrthogonalNeighbor (N, E, S, W). Loop deltas directly so a null
    // Y-edge neighbor does not shift later direction bits.
    static constexpr int k_Deltas[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    static constexpr RiverConnection_t k_Dirs[4] = {
        RiverConnection_t::North,
        RiverConnection_t::East,
        RiverConnection_t::South,
        RiverConnection_t::West,
    };

    RiverConnection_t mask = RiverConnection_t::None;
    for (int i = 0; i < 4; ++i)
    {
        const Tile* pNeighbor =
            rWorld.GetTile(rTile.GetX() + k_Deltas[i][0], rTile.GetY() + k_Deltas[i][1]);
        if (pNeighbor && pNeighbor->GetHasRiver())
        {
            mask |= k_Dirs[i];
        }
    }
    return mask;
}

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
