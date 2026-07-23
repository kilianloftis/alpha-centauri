#include "game/map/WorldMap.h"
#include "game/map/MapUtils.h"
#include "game/units/Unit.h"

namespace ac
{

WorldMap::WorldMap(int width, int height)
    : m_width(width)
    , m_height(height)
{
    m_tiles.reserve(width * height);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            m_tiles.push_back(std::make_unique<Tile>(x, y));
        }
    }
    m_territory.Reset(width, height);
}

WorldMap::~WorldMap()
{
}

int WorldMap::GetWidth() const
{
    return m_width;
}

int WorldMap::GetHeight() const
{
    return m_height;
}

Tile* WorldMap::GetTile(int x, int y)
{
    if (y < 0 || y >= m_height || m_width <= 0)
    {
        return nullptr;
    }
    x = WrapX(x, m_width);
    return m_tiles[GetTileIndex(x, y)].get();
}

const Tile* WorldMap::GetTile(int x, int y) const
{
    if (y < 0 || y >= m_height || m_width <= 0)
    {
        return nullptr;
    }
    x = WrapX(x, m_width);
    return m_tiles[GetTileIndex(x, y)].get();
}

std::vector<std::unique_ptr<Tile>>& WorldMap::GetTiles()
{
    return m_tiles;
}

const std::vector<std::unique_ptr<Tile>>& WorldMap::GetTiles() const
{
    return m_tiles;
}

int WorldMap::GetTileIndex(int x, int y) const
{
    return y * m_width + x;
}

int WorldMap::GetTileIndex(const Tile& rTile) const
{
    return GetTileIndex(rTile.GetX(), rTile.GetY());
}

UnitPositionIndex& WorldMap::GetUnitPositions()
{
    return m_unitPositionIndex;
}

const UnitPositionIndex& WorldMap::GetUnitPositions() const
{
    return m_unitPositionIndex;
}

const std::vector<Unit*>& WorldMap::GetUnitsOnTile(const Tile& rTile) const
{
    return m_unitPositionIndex.GetUnitsOnTile(rTile);
}

WorkedTileIndex& WorldMap::GetWorkedTiles()
{
    return m_workedTiles;
}

const WorkedTileIndex& WorldMap::GetWorkedTiles() const
{
    return m_workedTiles;
}

TerritoryMap& WorldMap::GetTerritory()
{
    return m_territory;
}

const TerritoryMap& WorldMap::GetTerritory() const
{
    return m_territory;
}

} // namespace ac
