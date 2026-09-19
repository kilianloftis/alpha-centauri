#include "game/units/EvacuateTerritoryEffects.h"

#include "game/Faction.h"
#include "game/faction/UnitManager.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/EvacuateTerritoryRules.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"

#include <unordered_map>
#include <vector>

namespace ac
{

namespace
{

using FreeByOrigin_t = std::unordered_map<const Tile*, std::vector<Unit*>>;

std::vector<Unit*> CollectUnitsOnHostTerritory_(Faction& rGuest,
                                                FactionId_t hostTerritoryOwner,
                                                const TerritoryMap& rTerritory)
{
    std::vector<Unit*> onHost;
    for (Unit& rUnit : rGuest.GetUnitManager().Units())
    {
        if (IsOnTerritoryOf(rUnit.GetTile(), hostTerritoryOwner, rTerritory))
        {
            onHost.push_back(&rUnit);
        }
    }
    return onHost;
}

// Clears orders on every on-host unit. Groups non-embarked units by their origin tile.
FreeByOrigin_t ClearOrdersAndGroupFreeByOrigin_(const std::vector<Unit*>& rOnHost)
{
    FreeByOrigin_t freeByOrigin;
    for (Unit* pUnit : rOnHost)
    {
        pUnit->ClearOrder();
        if (!pUnit->IsEmbarked())
        {
            freeByOrigin[&pUnit->GetTile()].push_back(pUnit);
        }
    }
    return freeByOrigin;
}

bool TryMoveUnitTo_(Unit& rUnit, const Tile& rDest, WorldMap& rWorldMap,
                    const InteractionGridsConfig_t& rGrids)
{
    if (!CanHoldTileWithoutCarrier(rUnit, rDest, rWorldMap, rGrids))
    {
        return false;
    }
    UnitPositionIndex& rPositions = rWorldMap.GetUnitPositions();
    if (!CanPlaceUnitOnTile(rDest, rPositions))
    {
        return false;
    }
    rPositions.MoveUnit(rUnit, rDest);
    return true;
}

void RelocateUnitsIndividually_(std::vector<Unit*>& rUnits, WorldMap& rWorldMap,
                                const InteractionGridsConfig_t& rGrids,
                                EvacuateTerritoryResult_t& rResult)
{
    for (Unit* pUnit : rUnits)
    {
        const Tile* pDest = FindNearestOwnTerritoryTile(*pUnit, rWorldMap, rGrids);
        if (!pDest)
        {
            ++rResult.unitsLeftInPlace;
            continue;
        }
        rWorldMap.GetUnitPositions().MoveUnit(*pUnit, *pDest);
        ++rResult.unitsMoved;
    }
}

// Moves every pending unit that can hold and place on rDest. Returns those that could not.
std::vector<Unit*> MoveGroupToDestination_(std::vector<Unit*>& rPending, const Tile& rDest,
                                           WorldMap& rWorldMap,
                                           const InteractionGridsConfig_t& rGrids,
                                           EvacuateTerritoryResult_t& rResult)
{
    std::vector<Unit*> remaining;
    remaining.reserve(rPending.size());
    for (Unit* pUnit : rPending)
    {
        if (TryMoveUnitTo_(*pUnit, rDest, rWorldMap, rGrids))
        {
            ++rResult.unitsMoved;
        }
        else
        {
            remaining.push_back(pUnit);
        }
    }
    return remaining;
}

void EvacuateOriginGroup_(std::vector<Unit*> pending, WorldMap& rWorldMap,
                          const InteractionGridsConfig_t& rGrids,
                          EvacuateTerritoryResult_t& rResult)
{
    while (!pending.empty())
    {
        const Tile* pDest = FindNearestOwnTerritoryTile(*pending.front(), rWorldMap, rGrids);
        if (!pDest)
        {
            rResult.unitsLeftInPlace += static_cast<int>(pending.size());
            return;
        }

        std::vector<Unit*> remaining =
            MoveGroupToDestination_(pending, *pDest, rWorldMap, rGrids, rResult);
        if (remaining.size() == pending.size())
        {
            RelocateUnitsIndividually_(remaining, rWorldMap, rGrids, rResult);
            return;
        }
        pending = std::move(remaining);
    }
}

} // namespace

EvacuateTerritoryResult_t EvacuateUnitsFromTerritory(
    Faction& rGuest, FactionId_t hostTerritoryOwner, WorldMap& rWorldMap,
    const InteractionGridsConfig_t& rGrids)
{
    EvacuateTerritoryResult_t result;
    const std::vector<Unit*> onHost =
        CollectUnitsOnHostTerritory_(rGuest, hostTerritoryOwner, rWorldMap.GetTerritory());
    FreeByOrigin_t freeByOrigin = ClearOrdersAndGroupFreeByOrigin_(onHost);

    for (auto& [pOrigin, units] : freeByOrigin)
    {
        (void)pOrigin;
        EvacuateOriginGroup_(std::move(units), rWorldMap, rGrids, result);
    }
    return result;
}

} // namespace ac
