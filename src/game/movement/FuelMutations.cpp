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

bool HasBoardableCarrier_(const WorldMap& rWorldMap, const Tile& rTile, const Unit& rPassenger)
{
    return std::ranges::any_of(rWorldMap.GetUnitsOnTile(rTile), [&rPassenger](const Unit* pOther)
    {
        return pOther && pOther != &rPassenger && CanCarryPassenger(*pOther, rPassenger);
    });
}

// The candidate reachable this turn at the lowest cost; the first wins a tie.
const Tile* CheapestReachable_(const Unit& rUnit, const Pathfinder& rPathfinder,
                               const std::vector<const Tile*>& rCandidates)
{
    const int remainingFragments = rUnit.GetMoveFragmentsRemaining();
    const Tile* pBest = nullptr;
    int bestCost = INT_MAX;
    for (const Tile* pDest : rCandidates)
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
    return pBest;
}

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
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, rangePoints,
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

            if (HasBoardableCarrier_(rWorldMap, *pTile, rUnit))
            {
                rOut.push_back(pTile);
            }
        });
}

} // namespace

bool TryAssignAutoReturnToFuel(Unit& rUnit, const Pathfinder& rPathfinder)
{
    const WorldMap& rWorldMap = rPathfinder.GetWorldMap();
    if (!NeedsAutoReturnToFuel(rUnit))
    {
        return false;
    }

    std::vector<const Tile*> candidates;
    CollectFriendlyRefuelTiles_(rUnit, rWorldMap, candidates);

    const Tile* pBest = CheapestReachable_(rUnit, rPathfinder, candidates);
    if (!pBest)
    {
        return false;
    }

    rUnit.SetOrder(MoveOrder_t{pBest});
    return true;
}

void ProcessFuelAtTurnEnd(Unit& rUnit)
{
    const EndOfTurnFuel_t fuel = ResolveEndOfTurnFuel(rUnit);
    if (fuel.bBoardsCarrier)
    {
        TryAttachToTransport(rUnit);
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
    for (Faction& rFaction : rGameState.Factions())
    {
        UnitManager& rUnits = rFaction.GetUnitManager();
        const auto destructionScope = rUnits.DeferDestruction();
        for (Unit& rUnit : rUnits.Units())
        {
            ProcessFuelAtTurnEnd(rUnit);
        }
    }
}

} // namespace ac
