#include "game/faction/FactionVisibilityMap.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include <algorithm>

namespace ac
{

namespace
{

// A base sees a Chebyshev radius-2 square (own tile plus two rings).
constexpr int k_BaseVisionRadius = 2;

} // namespace

void FactionVisibilityMap::Reset(int width, int height)
{
    m_width = width;
    m_height = height;
    const size_t count = (width > 0 && height > 0)
        ? static_cast<size_t>(width) * static_cast<size_t>(height)
        : 0;
    m_explored.assign(count, 0);
    m_visible.assign(count, 0);
    m_revision.Bump();
}

bool FactionVisibilityMap::InBounds_(int x, int y) const
{
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

size_t FactionVisibilityMap::Index_(int x, int y) const
{
    return static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
}

bool FactionVisibilityMap::IsExplored(int x, int y) const
{
    return IsSized() && InBounds_(x, y) && m_explored[Index_(x, y)] != 0;
}

bool FactionVisibilityMap::IsVisible(int x, int y) const
{
    return IsSized() && InBounds_(x, y) && m_visible[Index_(x, y)] != 0;
}

bool FactionVisibilityMap::IsExplored(const Tile& rTile) const
{
    return IsExplored(rTile.GetX(), rTile.GetY());
}

bool FactionVisibilityMap::IsVisible(const Tile& rTile) const
{
    return IsVisible(rTile.GetX(), rTile.GetY());
}

void FactionVisibilityMap::RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap)
{
    if (radius < 0)
    {
        return;
    }

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius, /*includeOrigin=*/true,
        [this](const Tile* pTile, int /*distance*/)
        {
            const size_t i = Index_(pTile->GetX(), pTile->GetY());
            m_visible[i] = 1;
            m_explored[i] = 1;
        });
}

void FactionVisibilityMap::RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap)
{
    if (!IsSized())
    {
        return;
    }

    std::fill(m_visible.begin(), m_visible.end(), 0);

    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        RevealAround_(rUnit.GetTile(), rUnit.GetVision(), rWorldMap);
    }

    for (const BaseManager& rBase : rFaction.Bases())
    {
        const Tile* pBaseTile = rWorldMap.GetTile(rBase.GetX(), rBase.GetY());
        if (pBaseTile)
        {
            RevealAround_(*pBaseTile, k_BaseVisionRadius, rWorldMap);
        }
    }

    m_revision.Bump();
}

} // namespace ac
