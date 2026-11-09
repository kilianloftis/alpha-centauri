#pragma once

#include "game/base/BaseTypes.h"

namespace ac
{

class Faction;
class GameState;
class Unit;
class WorldMap;

struct EvacuateTerritoryResult_t
{
    int unitsMoved = 0;
    int unitsLeftInPlace = 0;
};

// Clears orders; free units move to the nearest holdable own tile. Embarked cargo only clears
// orders (the carrier tows them).
EvacuateTerritoryResult_t EvacuateUnit(Unit& rUnit, WorldMap& rWorldMap);

// Selects rGuest units on hostTerritoryOwner's territory (except Covert the host cannot identify)
// and runs EvacuateUnit on each.
EvacuateTerritoryResult_t EvacuateUnitsFromTerritory(Faction& rGuest,
                                                     FactionId_t hostTerritoryOwner,
                                                     WorldMap& rWorldMap);

// Selects rGuest units sharing a tile with rHost (or in its bases) and runs EvacuateUnit on each.
EvacuateTerritoryResult_t EvacuateUnitsSharingWith(Faction& rGuest, const Faction& rHost,
                                                   WorldMap& rWorldMap);

// No-op while EvacuateUnitsOnTerritoryChange is false. Otherwise relocates every unit that
// MustVacateCurrentTerritory (territory redraw) or one unit after identity/permission change.
EvacuateTerritoryResult_t ApplyEvacuateOnTerritoryChange(GameState& rGameState);
EvacuateTerritoryResult_t ApplyEvacuateOnTerritoryChange(Unit& rUnit, WorldMap& rWorldMap);

} // namespace ac
