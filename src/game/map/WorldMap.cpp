#include "game/map/WorldMap.h"
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
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        return nullptr;
    }
    return m_tiles[GetTileIndex_(x, y)].get();
}

const Tile* WorldMap::GetTile(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        return nullptr;
    }
    return m_tiles[GetTileIndex_(x, y)].get();
}

std::vector<std::unique_ptr<Tile>>& WorldMap::GetTiles()
{
    return m_tiles;
}

const std::vector<std::unique_ptr<Tile>>& WorldMap::GetTiles() const
{
    return m_tiles;
}

int WorldMap::GetTileIndex_(int x, int y) const
{
    return y * m_width + x;
}

const std::vector<Unit*>& WorldMap::GetUnitsOnTile(const Tile& rTile) const
{
    return m_unitPositionIndex.GetUnitsOnTile(rTile);
}

void WorldMap::OnUnitPlaced(Unit& rUnit, const Tile& rTile)
{
    m_unitPositionIndex.OnUnitPlaced(rUnit, rTile);
    rUnit.SetTile(rTile);
}

void WorldMap::OnUnitRemoved(Unit& rUnit)
{
    m_unitPositionIndex.OnUnitRemoved(rUnit);
}

void WorldMap::OnUnitMoved(Unit& rUnit, const Tile& rNewTile)
{
    m_unitPositionIndex.OnUnitMoved(rUnit, rNewTile);
    rUnit.SetTile(rNewTile);
}

WorkedTileIndex& WorldMap::GetWorkedTiles()
{
    return m_workedTiles;
}

const WorkedTileIndex& WorldMap::GetWorkedTiles() const
{
    return m_workedTiles;
}

} // namespace ac
