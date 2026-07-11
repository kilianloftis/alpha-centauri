#pragma once

#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/TerritoryMap.h"
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

    // Unit occupancy: the single owner of unit-position state. Units register/unregister
    // themselves at construction/destruction; movement goes through
    // UnitPositionIndex::TryMoveUnit (see UnitPositionIndex).
    UnitPositionIndex& GetUnitPositions();
    const UnitPositionIndex& GetUnitPositions() const;
    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;

    // Worked-tile occupancy: the single owner of the one-worker-per-tile rule, shared by
    // every base of every faction (see WorkedTileIndex).
    WorkedTileIndex& GetWorkedTiles();
    const WorkedTileIndex& GetWorkedTiles() const;

    // Faction territory: mutually exclusive ownership derived from bases (see TerritoryMap).
    TerritoryMap& GetTerritory();
    const TerritoryMap& GetTerritory() const;

private:
    int m_width;
    int m_height;
    std::vector<std::unique_ptr<Tile>> m_tiles;
    UnitPositionIndex m_unitPositionIndex;
    WorkedTileIndex m_workedTiles;
    TerritoryMap m_territory;

    int GetTileIndex_(int x, int y) const;
};

} // namespace ac
