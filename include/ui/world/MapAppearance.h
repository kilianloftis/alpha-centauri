#pragma once

#include "game/map/TileOccupants.h"

namespace ac
{

class Faction;
class FactionTileMemory;
class FactionVisibleMap;
class Tile;
class WorldMap;

// The occupants a viewer sees: live on tiles in sight, remembered elsewhere.
class MapAppearance
{
public:
    // A null visible map or memory means live everywhere.
    MapAppearance(const WorldMap& rMap, const FactionVisibleMap* pVisible,
                  const FactionTileMemory* pMemory);

    const WorldMap& Map() const { return m_rMap; }
    TileOccupants_t OccupantsOf(const Tile& rTile) const;

private:
    const WorldMap& m_rMap;
    const FactionVisibleMap* m_pVisible;
    const FactionTileMemory* m_pMemory;
};

// What pFaction sees of rMap; live everywhere when there is no faction.
MapAppearance AppearanceOf(const WorldMap& rMap, const Faction* pFaction);

} // namespace ac
