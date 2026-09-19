#pragma once

#include "game/faction/base/BaseTypes.h"

namespace ac
{

class Faction;
class WorldMap;
struct InteractionGridsConfig_t;

struct EvacuateTerritoryResult_t
{
    int unitsMoved = 0;
    int unitsLeftInPlace = 0;
};

// Clear orders on every free or embarked unit of rGuest whose tile is owned by
// hostTerritoryOwner. Relocate free units to the nearest holdable own-territory tile (no
// move spend), sharing one destination search per origin tile when stacks share a tile.
// Embarked cargo is towed when its carrier moves and is not counted separately. Units with
// no legal destination stay put (orders still cleared).
EvacuateTerritoryResult_t EvacuateUnitsFromTerritory(
    Faction& rGuest, FactionId_t hostTerritoryOwner, WorldMap& rWorldMap,
    const InteractionGridsConfig_t& rGrids);

} // namespace ac
