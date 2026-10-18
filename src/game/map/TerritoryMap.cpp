#include "game/map/TerritoryMap.h"

#include "game/map/MapUtils.h"
#include "game/map/TerritoryRules.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace ac
{

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

    m_owners = ClaimTerritory(rWorldMap, rBases);
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
