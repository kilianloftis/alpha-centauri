#pragma once

#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/TerritoryMap.h"
#include <memory>
#include <span>
#include <vector>

namespace ac
{

class Unit;

class WorldMap
{
public:
    // Throws unless both dimensions are positive: a zero-sized map has no valid tile.
    WorldMap(int width, int height);
    ~WorldMap();

    // Map dimensions
    int GetWidth() const;
    int GetHeight() const;

    // Tile access. X wraps horizontally (cylinder); Y out of bounds returns nullptr.
    Tile* GetTile(int x, int y);
    const Tile* GetTile(int x, int y) const;

    // Row-major index into GetTiles() for an in-bounds (x, y). Does not bounds-check.
    int GetTileIndex(int x, int y) const;
    int GetTileIndex(const Tile& rTile) const;

    // All tiles, row-major. Const-element span, not the owning vector: units, bases,
    // UnitPositionIndex and WorkedTileIndex all hold raw Tile*, so tile addresses must stay put
    // for the lifetime of the map. Tiles remain mutable through the pointer; reseating and
    // clearing are what the span forbids.
    std::span<const std::unique_ptr<Tile>> GetTiles() const;

    // Unit occupancy: the single owner of unit-position state. Units register/unregister
    // themselves at construction/destruction; movement goes through
    // UnitPositionIndex::MoveUnit (see UnitPositionIndex).
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
};

} // namespace ac
