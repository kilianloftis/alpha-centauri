#pragma once

#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include <memory>
#include <vector>

namespace ac
{

class Unit;

class WorldMap
{
public:
    WorldMap(int width, int height);
    ~WorldMap();

    // Map dimensions
    int GetWidth() const;
    int GetHeight() const;

    // Tile access
    Tile* GetTile(int x, int y);
    const Tile* GetTile(int x, int y) const;

    // Get all tiles
    std::vector<std::unique_ptr<Tile>>& GetTiles();
    const std::vector<std::unique_ptr<Tile>>& GetTiles() const;

    // Unit position tracking
    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;
    void OnUnitPlaced(Unit& rUnit, const Tile& rTile);
    void OnUnitRemoved(Unit& rUnit);
    void OnUnitMoved(Unit& rUnit, const Tile& rNewTile);

    // Worked-tile occupancy: the single owner of the one-worker-per-tile rule, shared by
    // every base of every faction (see WorkedTileIndex).
    WorkedTileIndex& GetWorkedTiles();
    const WorkedTileIndex& GetWorkedTiles() const;

private:
    int m_width;
    int m_height;
    std::vector<std::unique_ptr<Tile>> m_tiles;
    UnitPositionIndex m_unitPositionIndex;
    WorkedTileIndex m_workedTiles;

    int GetTileIndex_(int x, int y) const;
};

} // namespace ac
