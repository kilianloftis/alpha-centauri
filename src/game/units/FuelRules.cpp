#include "game/units/FuelRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/UnitManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementConstants.h"
#include "game/units/Pathfinder.h"
#include "game/units/TransportRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitOrder.h"

#include <climits>
#include <vector>

namespace ac
{

namespace
{

bool UnitProjectsRefuelsAir_(const Unit& rUnit)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit))
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::RefuelsAir
            && rEffect.config->scope == EffectScope_t::ThisTile
            && rEffect.config->radius == 0
            && !rEffect.config->condition.has_value())
        {
            return true;
        }
    }
    return false;
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

    // One disk pass: territory pads, plus boardable RefuelsAir carriers already in range
    // (GetUnitsOnTile) instead of scanning the whole faction roster.
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, rangePoints, /*includeOrigin=*/true,
        [&](const Tile* pTile, int /*distance*/)
        {
            if (!pTile)
            {
                return;
            }

            if (ResolveFlag(*pTile, RuleFlagId_t::RefuelsAir)
                && rWorldMap.GetTerritory().GetOwner(*pTile) == factionId)
            {
                rOut.push_back(pTile);
            }

            for (const Unit* pOccupant : rWorldMap.GetUnitsOnTile(*pTile))
            {
                if (!pOccupant || pOccupant == &rUnit || !UnitProjectsRefuelsAir_(*pOccupant))
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

bool IsRefuelSite(const Unit& rUnit)
{
    // Pad improvements (Base, Airbase): any unit ending the turn on the tile.
    if (ResolveFlag(rUnit.GetTile(), RuleFlagId_t::RefuelsAir))
    {
        return true;
    }

    // Carrier decks (and similar unit-projected pads): only landed cargo.
    if (!rUnit.IsEmbarked())
    {
        return false;
    }
    const Unit* pCarrier = rUnit.GetCarrier();
    return pCarrier && UnitProjectsRefuelsAir_(*pCarrier);
}

namespace
{

int OutOfFuelDamage_(const Unit& rUnit)
{
    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    const int damagePercent = ResolveStat(rUnit, StatId_t::DamageFromOutOfFuel);
    return FinalizeResolvedStat(maxHp * (damagePercent / 100.0));
}

// Mirrors ProcessFuelAtTurnEnd's away-from-pad outcome without mutating: after optional
// carrier land, would remaining-move fuel burn hit 0 and out-of-fuel damage kill the unit?
bool WouldBeDestroyedWithoutRefuelThisTurn_(const Unit& rUnit, const WorldMap& rWorldMap)
{
    if (!rUnit.GetDesign().UsesFuel())
    {
        return false;
    }

    if (!rUnit.IsEmbarked() && rUnit.GetDomain() == UnitDomain_t::Air
        && FindBoardableTransport(rUnit, rUnit.GetTile(), rWorldMap))
    {
        return false;
    }

    if (IsRefuelSite(rUnit))
    {
        return false;
    }

    const int movesRemaining =
        rUnit.GetMoveFragmentsRemaining() / MovementConstants_t::k_moveFragmentsPerPoint;
    if (rUnit.GetCurrentFuel() - movesRemaining > 0)
    {
        return false;
    }

    return rUnit.GetCurrentHp() - OutOfFuelDamage_(rUnit) <= 0;
}

} // namespace

bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap)
{
    if (rUnit.GetOrder().has_value() || rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }

    return WouldBeDestroyedWithoutRefuelThisTurn_(rUnit, rWorldMap);
}

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
    if (!rUnit.GetDesign().UsesFuel())
    {
        return;
    }

    // Land on a carrier before fuel accounting so a free deck slot can save the unit.
    if (!rUnit.IsEmbarked() && rUnit.GetDomain() == UnitDomain_t::Air)
    {
        TryAttachToTransport(rUnit, rWorldMap);
    }
    
    if (IsRefuelSite(rUnit))
    {
        rUnit.SetCurrentFuel(rUnit.GetMaxFuel());
        return;
    }

    // Unused moves still consume fuel for the airborne turn.
    rUnit.SpendRemainingMoveFragments();

    if (rUnit.GetCurrentFuel() > 0)
    {
        return;
    }

    const int damage = OutOfFuelDamage_(rUnit);
    rUnit.SetCurrentHp(rUnit.GetCurrentHp() - damage);
    if (rUnit.GetCurrentHp() <= 0)
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
