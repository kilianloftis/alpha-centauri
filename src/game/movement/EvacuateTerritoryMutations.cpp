#include "game/movement/EvacuateTerritoryMutations.h"

#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/units/UnitManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/movement/EvacuateTerritoryRules.h"
#include "game/units/Unit.h"

#include <vector>

namespace ac
{

namespace
{

void AddResult_(EvacuateTerritoryResult_t& rTotal, const EvacuateTerritoryResult_t& rOne)
{
    rTotal.unitsMoved += rOne.unitsMoved;
    rTotal.unitsLeftInPlace += rOne.unitsLeftInPlace;
}

EvacuateTerritoryResult_t EvacuateEach_(const std::vector<Unit*>& rUnits, WorldMap& rWorldMap)
{
    EvacuateTerritoryResult_t result;
    for (Unit* pUnit : rUnits)
    {
        AddResult_(result, EvacuateUnit(*pUnit, rWorldMap));
    }
    return result;
}

} // namespace

EvacuateTerritoryResult_t EvacuateUnit(Unit& rUnit, WorldMap& rWorldMap)
{
    EvacuateTerritoryResult_t result;
    rUnit.ClearOrder();
    // Embarked cargo clears orders here; the carrier's MoveUnit tows it.
    if (rUnit.Transport().IsEmbarked())
    {
        return result;
    }
    const Tile* pDest = FindNearestOwnTerritoryTile(rUnit);
    if (!pDest)
    {
        result.unitsLeftInPlace = 1;
        return result;
    }
    rWorldMap.GetUnitPositions().MoveUnit(rUnit, *pDest);
    result.unitsMoved = 1;
    return result;
}

EvacuateTerritoryResult_t EvacuateUnitsFromTerritory(Faction& rGuest,
                                                     FactionId_t hostTerritoryOwner,
                                                     WorldMap& rWorldMap)
{
    std::vector<Unit*> onHost;
    for (Unit& rUnit : rGuest.GetUnitManager().Units())
    {
        if (MustLeaveTerritory(rUnit, hostTerritoryOwner, rWorldMap.GetTerritory()))
        {
            onHost.push_back(&rUnit);
        }
    }
    return EvacuateEach_(onHost, rWorldMap);
}

EvacuateTerritoryResult_t EvacuateUnitsSharingWith(Faction& rGuest, const Faction& rHost,
                                                   WorldMap& rWorldMap)
{
    std::vector<Unit*> sharing;
    for (Unit& rUnit : rGuest.GetUnitManager().Units())
    {
        if (SharesTileWith(rUnit, rHost))
        {
            sharing.push_back(&rUnit);
        }
    }
    return EvacuateEach_(sharing, rWorldMap);
}

EvacuateTerritoryResult_t ApplyEvacuateOnTerritoryChange(GameState& rGameState)
{
    if (!EvacuateUnitsOnTerritoryChange())
    {
        return {};
    }
    std::vector<Unit*> illegal;
    for (Faction& rFaction : rGameState.Factions())
    {
        for (Unit& rUnit : rFaction.GetUnitManager().Units())
        {
            if (MustVacateCurrentTerritory(rUnit))
            {
                illegal.push_back(&rUnit);
            }
        }
    }
    return EvacuateEach_(illegal, rGameState.GetWorldMap());
}

EvacuateTerritoryResult_t ApplyEvacuateOnTerritoryChange(Unit& rUnit, WorldMap& rWorldMap)
{
    if (!EvacuateUnitsOnTerritoryChange() || !MustVacateCurrentTerritory(rUnit))
    {
        return {};
    }
    return EvacuateUnit(rUnit, rWorldMap);
}

} // namespace ac
