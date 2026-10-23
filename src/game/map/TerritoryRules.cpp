#include "game/map/TerritoryRules.h"

#include "game/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <limits>
#include <queue>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

constexpr int k_LandTerritoryRadius = 7;
constexpr int k_SeaTerritoryRadius = 3;

struct ClaimCandidate_t
{
    FactionId_t factionId = k_NoFactionOwner;
    int distSq = std::numeric_limits<int>::max();
    BaseId_t baseId = std::numeric_limits<BaseId_t>::max();
};

bool Beats_(const ClaimCandidate_t& rChallenger, const ClaimCandidate_t& rIncumbent)
{
    if (rChallenger.distSq != rIncumbent.distSq)
    {
        return rChallenger.distSq < rIncumbent.distSq;
    }
    return rChallenger.baseId < rIncumbent.baseId;
}

int LatticeDistSq_(const Tile& rA, const Tile& rB, int mapWidth)
{
    const LatticeDelta_t d = LatticeDelta(rA, rB, mapWidth);
    return d.p * d.p + d.q * d.q;
}

void RequireOnGrid_(const BaseManager& rBase, int width, int height)
{
    const int ox = rBase.GetTile().GetX();
    const int oy = rBase.GetTile().GetY();
    if (oy < 0 || oy >= height || ox < 0 || ox >= width)
    {
        throw std::out_of_range("TerritoryMap: base " + std::to_string(rBase.GetBaseId())
                                + " sits at (" + std::to_string(ox) + ", " + std::to_string(oy)
                                + "), outside the " + std::to_string(width) + "x"
                                + std::to_string(height) + " territory grid");
    }
}

// Tiles of the origin's own surface within its territory radius, reached without crossing the
// other surface.
std::vector<const Tile*> TerritoryReach_(const Tile& rOrigin, const WorldMap& rWorldMap,
                                         int width, int height)
{
    const bool bWantWater = rOrigin.IsWater();
    const int radius = bWantWater ? k_SeaTerritoryRadius : k_LandTerritoryRadius;
    const size_t tileCount = static_cast<size_t>(width) * static_cast<size_t>(height) / 2;
    std::vector<uint8_t> visited(tileCount, 0);
    std::vector<const Tile*> reached;
    std::queue<const Tile*> queue;
    queue.push(&rOrigin);
    visited[static_cast<size_t>(TileIndex(rOrigin.GetX(), rOrigin.GetY(), width))] = 1;
    while (!queue.empty())
    {
        const Tile* pCurrent = queue.front();
        queue.pop();
        reached.push_back(pCurrent);
        ForEachOrthogonalNeighbor(*pCurrent, rWorldMap, [&](const Tile* pNeighbor) {
            const int neighborIndex = TileIndex(pNeighbor->GetX(), pNeighbor->GetY(), width);
            const size_t nIdx = static_cast<size_t>(neighborIndex);
            const LatticeDelta_t d = LatticeDelta(rOrigin, *pNeighbor, width);
            if (visited[nIdx] || !InEuclideanRadius(d.p, d.q, radius)
                || pNeighbor->IsWater() != bWantWater)
            {
                return;
            }
            visited[nIdx] = 1;
            queue.push(pNeighbor);
        });
    }
    return reached;
}

void ClaimFromBase_(const BaseManager& rBase, const WorldMap& rWorldMap,
                    std::vector<ClaimCandidate_t>& rBest, int width, int height)
{
    RequireOnGrid_(rBase, width, height);
    const Tile& rOrigin = rBase.GetTile();
    for (const Tile* pTile : TerritoryReach_(rOrigin, rWorldMap, width, height))
    {
        const ClaimCandidate_t challenger{rBase.GetFactionId(),
                                          LatticeDistSq_(rOrigin, *pTile, width),
                                          rBase.GetBaseId()};
        const int index = TileIndex(pTile->GetX(), pTile->GetY(), width);
        ClaimCandidate_t& rIncumbent = rBest[static_cast<size_t>(index)];
        if (rIncumbent.factionId == k_NoFactionOwner || Beats_(challenger, rIncumbent))
        {
            rIncumbent = challenger;
        }
    }
}

} // namespace

std::vector<FactionId_t> ClaimTerritory(const WorldMap& rWorldMap,
                                        const std::vector<const BaseManager*>& rBases)
{
    const int width = rWorldMap.GetWidth();
    const int height = rWorldMap.GetHeight();
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height) / 2;
    std::vector<ClaimCandidate_t> best(count);
    for (const BaseManager* pBase : rBases)
    {
        ClaimFromBase_(*pBase, rWorldMap, best, width, height);
    }

    std::vector<FactionId_t> owners(count, k_NoFactionOwner);
    for (size_t i = 0; i < count; ++i)
    {
        owners[i] = best[i].factionId;
    }
    return owners;
}

} // namespace ac
