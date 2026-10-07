#include "game/map/TerritoryMap.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>

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

void ClaimFromBase_(const BaseManager& rBase, const WorldMap& rWorldMap,
                    std::vector<ClaimCandidate_t>& rBest, int width, int height)
{
    const Tile& rOrigin = rBase.GetTile();

    const bool bWantWater = rOrigin.IsWater();
    const int radius = bWantWater ? k_SeaTerritoryRadius : k_LandTerritoryRadius;
    const FactionId_t factionId = rBase.GetFactionId();
    const BaseId_t baseId = rBase.GetBaseId();
    const int ox = rOrigin.GetX();
    const int oy = rOrigin.GetY();

    if (oy < 0 || oy >= height || ox < 0 || ox >= width)
    {
        throw std::out_of_range("TerritoryMap: base " + std::to_string(baseId) + " sits at ("
                                + std::to_string(ox) + ", " + std::to_string(oy)
                                + "), outside the " + std::to_string(width) + "x"
                                + std::to_string(height) + " territory grid");
    }

    const size_t tileCount = static_cast<size_t>(width) * static_cast<size_t>(height) / 2;
    std::vector<uint8_t> visited(tileCount, 0);

    std::queue<const Tile*> queue;
    queue.push(&rOrigin);
    visited[static_cast<size_t>(TileIndex(ox, oy, width))] = 1;

    while (!queue.empty())
    {
        const Tile* pCurrent = queue.front();
        queue.pop();

        const size_t idx = static_cast<size_t>(
            TileIndex(pCurrent->GetX(), pCurrent->GetY(), width));
        const ClaimCandidate_t challenger{factionId, LatticeDistSq_(rOrigin, *pCurrent, width),
                                          baseId};
        ClaimCandidate_t& rIncumbent = rBest[idx];
        if (rIncumbent.factionId == k_NoFactionOwner || Beats_(challenger, rIncumbent))
        {
            rIncumbent = challenger;
        }

        ForEachOrthogonalNeighbor(*pCurrent, rWorldMap, [&](const Tile* pNeighbor) {
            const size_t nIdx = static_cast<size_t>(
                TileIndex(pNeighbor->GetX(), pNeighbor->GetY(), width));
            if (visited[nIdx])
            {
                return;
            }
            const LatticeDelta_t d = LatticeDelta(rOrigin, *pNeighbor, width);
            if (!InEuclideanRadius(d.p, d.q, radius))
            {
                return;
            }
            if (pNeighbor->IsWater() != bWantWater)
            {
                return;
            }
            visited[nIdx] = 1;
            queue.push(pNeighbor);
        });
    }
}

} // namespace

void TerritoryMap::Reset(int width, int height)
{
    m_width = width;
    m_height = height;
    const size_t count = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * static_cast<size_t>(height) / 2
        : 0;
    m_owners.assign(count, k_NoFactionOwner);
    m_revision.Bump();
}

void TerritoryMap::Rebuild(const WorldMap& rWorldMap, const std::vector<const BaseManager*>& rBases)
{
    if (!IsSized())
    {
        throw std::logic_error("TerritoryMap::Rebuild called before Reset");
    }
    if (rWorldMap.GetWidth() != m_width || rWorldMap.GetHeight() != m_height)
    {
        throw std::logic_error(
            "TerritoryMap::Rebuild grid is " + std::to_string(m_width) + "x"
            + std::to_string(m_height) + " but the world is "
            + std::to_string(rWorldMap.GetWidth()) + "x" + std::to_string(rWorldMap.GetHeight()));
    }

    const size_t count = m_owners.size();
    std::vector<ClaimCandidate_t> best(count);
    for (const BaseManager* pBase : rBases)
    {
        if (pBase)
        {
            ClaimFromBase_(*pBase, rWorldMap, best, m_width, m_height);
        }
    }

    for (size_t i = 0; i < count; ++i)
    {
        m_owners[i] = best[i].factionId;
    }
    m_revision.Bump();
}

bool TerritoryMap::InBounds_(int x, int y) const
{
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

size_t TerritoryMap::Index_(int x, int y) const
{
    return static_cast<size_t>(TileIndex(x, y, m_width));
}

FactionId_t TerritoryMap::GetOwner(int x, int y) const
{
    if (!IsSized() || !InBounds_(x, y))
    {
        return k_NoFactionOwner;
    }
    return m_owners[Index_(x, y)];
}

FactionId_t TerritoryMap::GetOwner(const Tile& rTile) const
{
    return GetOwner(rTile.GetX(), rTile.GetY());
}

bool TerritoryMap::HasOwner(int x, int y) const
{
    return GetOwner(x, y) != k_NoFactionOwner;
}

bool TerritoryMap::HasOwner(const Tile& rTile) const
{
    return HasOwner(rTile.GetX(), rTile.GetY());
}

} // namespace ac
