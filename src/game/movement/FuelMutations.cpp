#include "game/movement/FuelMutations.h"

#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/WorldMap.h"
#include "game/movement/FuelRules.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/Pathfinder.h"
#include "game/movement/TransportRules.h"
#include "game/movement/TransportMutations.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitOrder.h"

#include <climits>
#include <vector>

namespace ac
{

namespace
{

// Pads / carriers the mover could reach this turn (Chebyshev radius = remaining move points).
// Pathfinding still decides real reachability and cost.
void CollectFriendlyRefuelTiles_(const Unit& rUnit, const WorldMap& rWorldMap,
                                 std::vector<const Tile*>& rOut)
{
    const int rangePoints =
        rUnit.GetMoveFragmentsRemaining() / MovementConstants_t::k_moveFragmentsPerPoint;
    if (rangePoints <= 0)
    {
        return;
    }

    const FactionId_t factionId = rUnit.GetFaction().GetFactionId();
    const Tile& rOrigin = rUnit.GetTile();
    const TerritoryMap& rTerritory = rWorldMap.GetTerritory();

    // One disk pass: territory pads, plus boardable carriers already in range
    // (GetUnitsOnTile) instead of scanning the whole faction roster.
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, rangePoints, /*includeOrigin=*/true,
        [&](const Tile* pTile, int /*distance*/)
        {
            if (!pTile)
            {
                return;
            }

            if (TileHarbors(*pTile, rUnit.GetDomain(), factionId, rTerritory))
            {
                rOut.push_back(pTile);
            }

            for (const Unit* pOccupant : rWorldMap.GetUnitsOnTile(*pTile))
            {
                if (!pOccupant || pOccupant == &rUnit)
                {
                    continue;
                }
                if (!CanCarryPassenger(*pOccupant, rUnit))
                {
                    continue;
                }
                rOut.push_back(pTile);
            }
        });
}

} // namespace

bool TryAssignAutoReturnToFuel(Unit& rUnit, const Pathfinder& rPathfinder)
{
    const WorldMap& rWorldMap = rPathfinder.GetWorldMap();
    if (!NeedsAutoReturnToFuel(rUnit, rWorldMap))
    {
        return false;
    }

    std::vector<const Tile*> candidates;
    CollectFriendlyRefuelTiles_(rUnit, rWorldMap, candidates);

    const int remainingFragments = rUnit.GetMoveFragmentsRemaining();
    const Tile* pBest = nullptr;
    int bestCost = INT_MAX;
    for (const Tile* pDest : candidates)
    {
        if (!pDest || pDest == &rUnit.GetTile())
        {
            continue;
        }

        const Path_t path = rPathfinder.FindPath(rUnit, *pDest);
        if (!path.bReachable || path.totalCostFragments > remainingFragments
            || path.totalCostFragments >= bestCost)
        {
            continue;
        }
        bestCost = path.totalCostFragments;
        pBest = pDest;
    }

    if (!pBest)
    {
        return false;
    }

    rUnit.SetOrder(MoveOrder_t{pBest});
    return true;
}

void ProcessFuelAtTurnEnd(Unit& rUnit, const WorldMap& rWorldMap)
{
    const EndOfTurnFuel_t fuel = ResolveEndOfTurnFuel(rUnit, rWorldMap);
    if (fuel.bBoardsCarrier)
    {
        TryAttachToTransport(rUnit, rWorldMap);
    }
    if (fuel.bRefuels)
    {
        rUnit.SetCurrentFuel(rUnit.GetMaxFuel());
        return;
    }
    if (!fuel.bBurnsFuel)
    {
        return;
    }
    rUnit.SpendRemainingMoveFragments();
    if (!fuel.bOutOfFuel)
    {
        return;
    }
    rUnit.SetCurrentHp(rUnit.GetCurrentHp() - fuel.outOfFuelDamage);
    if (fuel.bDestroyed)
    {
        rUnit.GetFaction().GetUnitManager().DestroyUnit(rUnit);
    }
}

void ProcessAllFuelAtTurnEnd(GameState& rGameState)
{
    WorldMap& rWorldMap = rGameState.GetWorldMap();
    for (Faction& rFaction : rGameState.Factions())
    {
        UnitManager& rUnits = rFaction.GetUnitManager();
        const auto destructionScope = rUnits.DeferDestruction();
        for (Unit& rUnit : rUnits.Units())
        {
            ProcessFuelAtTurnEnd(rUnit, rWorldMap);
        }
    }
}

} // namespace ac
