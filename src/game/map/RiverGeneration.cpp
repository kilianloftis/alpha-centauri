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
    return rTile.ForEachOccupant([](const ImprovementConfig_t& rConfig) {
        return rConfig.terminatesRiver;
    });
}

namespace
{

Tile* LowestNeighbor_(const Tile& rTile, WorldMap& rWorld)
{
    Tile* pLowest = nullptr;
    ForEachOrthogonalNeighbor(rTile, rWorld, [&](Tile* pNeighbor)
    {
        if (!pLowest || pNeighbor->GetElevation() < pLowest->GetElevation())
        {
            pLowest = pNeighbor;
        }
    });
    return pLowest;
}

} // namespace

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

        Tile* pBest = LowestNeighbor_(*pCurrent, rWorld);
        if (!pBest || pBest->GetElevation() >= pCurrent->GetElevation())
        {
            break;
        }
        pCurrent = pBest;
    }
}

std::vector<Tile*> RecomputeRivers(WorldMap& rWorld)
{
    TileChangeDeferral defer;

    std::vector<Tile*> tiles;
    std::vector<char> hadRiver;
    tiles.reserve(rWorld.GetTiles().size());
    hadRiver.reserve(rWorld.GetTiles().size());
    for (auto& pTile : rWorld.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        tiles.push_back(pTile.get());
        hadRiver.push_back(pTile->GetHasRiver() ? 1 : 0);
        pTile->SetHasRiver(false);
    }

    for (Tile* pTile : tiles)
    {
        if (pTile->GetHasAquifer())
        {
            TraceRiverFrom(*pTile, rWorld);
        }
    }

    std::vector<Tile*> changed;
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        const bool bHasRiver = tiles[i]->GetHasRiver();
        if (bHasRiver != (hadRiver[i] != 0))
        {
            changed.push_back(tiles[i]);
        }
    }
    return changed;
}

} // namespace ac
