#pragma once

#include "game/map/ElevationRulesConfig.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/TerritoryMap.h"
#include "lib/Revision.h"
#include <memory>
#include <span>
#include <vector>

namespace ac
{

class Unit;

class WorldMap
{
public:
    // Throws unless both dimensions are positive and width (the SMAC x wrap period) is even.
    // Immovable, since every tile points at its copy of rMapRules.
    WorldMap(int width, int height, const ElevationRulesConfig_t& rMapRules);
    WorldMap(const WorldMap&) = delete;
    WorldMap& operator=(const WorldMap&) = delete;
    WorldMap(WorldMap&&) = delete;
    WorldMap& operator=(WorldMap&&) = delete;
    ~WorldMap();

    int GetWidth() const;
    int GetHeight() const;
    // Integer square root of the tile count, terranx MapAreaSqRoot (0x949888). 56 on a standard
    // map; tech cost and the bureaucracy limit scale by it over 56.
    int GetAreaSqRoot() const;

    // Tile access in SMAC coordinates (x+y even). X wraps horizontally (cylinder);
    // Y out of bounds returns nullptr. Odd parity throws.
    Tile* GetTile(int x, int y);
    const Tile* GetTile(int x, int y) const;
    // This map's own tile at rTile's position, for callers that hold it const. Throws when no
    // tile is there.
    Tile& MutableTile(const Tile& rTile);

    // Row-major index into GetTiles() for an in-bounds even-parity (x, y). Throws on odd parity.
    int GetTileIndex(int x, int y) const;
    int GetTileIndex(const Tile& rTile) const;

    // Row-major. A const span, so tile addresses that units and indexes hold cannot move.
    std::span<const std::unique_ptr<Tile>> GetTiles() const;

    // The owner of unit positions.
    UnitPositionIndex& GetUnitPositions();
    const UnitPositionIndex& GetUnitPositions() const;
    // Occupants only (non-embarked). See UnitPositionIndex.
    const std::vector<Unit*>& GetUnitsOnTile(const Tile& rTile) const;
    const std::vector<Unit*>& GetCargoOnTile(const Tile& rTile) const;
    // Occupants + cargo (fresh vector). See UnitPositionIndex.
    std::vector<Unit*> GetAllUnitsOnTile(const Tile& rTile) const;

    // Worked-tile occupancy: the single owner of the one-worker-per-tile rule, shared by
    // every base of every faction (see WorkedTileIndex).
    WorkedTileIndex& GetWorkedTiles();
    const WorkedTileIndex& GetWorkedTiles() const;

    // Faction territory: mutually exclusive ownership derived from bases (see TerritoryMap).
    TerritoryMap& GetTerritory();
    const TerritoryMap& GetTerritory() const;

    // Bumped when a tile's minimap fill inputs change (elevation, fungus, improvements).
    uint64_t GetAppearanceRevision() const { return m_appearanceRevision.Get(); }

private:
    int m_width;
    int m_height;
    ElevationRulesConfig_t m_mapRules;
    std::vector<std::unique_ptr<Tile>> m_tiles;
    UnitPositionIndex m_unitPositionIndex;
    WorkedTileIndex m_workedTiles;
    TerritoryMap m_territory;
    Revision m_appearanceRevision;
};

} // namespace ac
