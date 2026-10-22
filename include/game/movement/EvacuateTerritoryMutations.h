#pragma once

#include "game/base/BaseTypes.h"

namespace ac
{

class Faction;
class WorldMap;

struct EvacuateTerritoryResult_t
{
    int unitsMoved = 0;
    int unitsLeftInPlace = 0;
};

// Clears orders on rGuest's units in hostTerritoryOwner's territory (but covert ones the host
// cannot identify) and moves free units to the nearest holdable own tile.
EvacuateTerritoryResult_t EvacuateUnitsFromTerritory(Faction& rGuest,
                                                     FactionId_t hostTerritoryOwner,
                                                     WorldMap& rWorldMap);

// The same relocation for every unit of rGuest that shares a tile with a unit of rHost or
// stands in one of rHost's bases.
EvacuateTerritoryResult_t EvacuateUnitsSharingWith(Faction& rGuest, const Faction& rHost,
                                                   WorldMap& rWorldMap);

} // namespace ac
