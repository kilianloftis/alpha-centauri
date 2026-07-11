#include "game/map/TerritoryMap.h"

#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <limits>
#include <queue>
#include <utility>

namespace ac
{

namespace
{

constexpr int k_LandTerritoryRadius = 7;
constexpr int k_SeaTerritoryRadius = 3;

struct ClaimCandidate_t
{
    FactionId factionId = k_NoFactionOwner;
    int distSq = std::numeric_limits<int>::max();
    int baseId = std::numeric_limits<int>::max();
};

bool Beats_(const ClaimCandidate_t& rChallenger, const ClaimCandidate_t& rIncumbent)
{
    if (rChallenger.distSq != rIncumbent.distSq)
    {
        return rChallenger.distSq < rIncumbent.distSq;
    }
    return rChallenger.baseId < rIncumbent.baseId;
}

int EuclideanDistSq_(int x0, int y0, int x1, int y1)
{
    const int dx = x0 - x1;
    const int dy = y0 - y1;
    return dx * dx + dy * dy;
}

void ClaimFromBase_(const BaseManager& rBase, const WorldMap& rWorldMap,
                    std::vector<ClaimCandidate_t>& rBest, int width, int height)
{
    const Tile* pOrigin = rWorldMap.GetTile(rBase.GetX(), rBase.GetY());
    if (!pOrigin)
    {
        return;
    }

    const bool bWantWater = pOrigin->IsWater();
    const int radius = bWantWater ? k_SeaTerritoryRadius : k_LandTerritoryRadius;
    const FactionId factionId = rBase.GetFactionId();
    const int baseId = rBase.GetBaseId();
    const int ox = pOrigin->GetX();
    const int oy = pOrigin->GetY();

    std::vector<uint8_t> visited(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    auto index = [width](int x, int y) {
        return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
    };

    std::queue<std::pair<int, int>> queue;
    queue.push({ox, oy});
    visited[index(ox, oy)] = 1;

    static constexpr int k_Dx[4] = {1, -1, 0, 0};
    static constexpr int k_Dy[4] = {0, 0, 1, -1};

    while (!queue.empty())
    {
        const auto [x, y] = queue.front();
        queue.pop();

        const ClaimCandidate_t challenger{factionId, EuclideanDistSq_(ox, oy, x, y), baseId};
        ClaimCandidate_t& rIncumbent = rBest[index(x, y)];
        if (rIncumbent.factionId == k_NoFactionOwner || Beats_(challenger, rIncumbent))
        {
            rIncumbent = challenger;
        }

        for (int i = 0; i < 4; ++i)
        {
            const int nx = x + k_Dx[i];
            const int ny = y + k_Dy[i];
            if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            {
                continue;
            }
            if (visited[index(nx, ny)])
            {
                continue;
            }
            if (!InEuclideanRadius(nx - ox, ny - oy, radius))
            {
                continue;
            }
            const Tile* pNeighbor = rWorldMap.GetTile(nx, ny);
            if (!pNeighbor || pNeighbor->IsWater() != bWantWater)
            {
                continue;
            }
            visited[index(nx, ny)] = 1;
            queue.push({nx, ny});
        }
    }
}

} // namespace

void TerritoryMap::Reset(int width, int height)
{
    m_width = width;
    m_height = height;
    const size_t count = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * static_cast<size_t>(height)
        : 0;
    m_owners.assign(count, k_NoFactionOwner);
    m_revision.Bump();
}

void TerritoryMap::Rebuild(const WorldMap& rWorldMap, const std::vector<const BaseManager*>& rBases)
{
    if (!IsSized())
    {
        return;
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
    return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
}

FactionId TerritoryMap::GetOwner(int x, int y) const
{
    if (!IsSized() || !InBounds_(x, y))
    {
        return k_NoFactionOwner;
    }
    return m_owners[Index_(x, y)];
}

FactionId TerritoryMap::GetOwner(const Tile& rTile) const
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
