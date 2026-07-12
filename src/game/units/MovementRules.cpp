#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/effects/EffectEnums.h"

namespace ac
{

namespace
{

bool IgnoresZoneOfControl_(const Unit& rUnit)
{
    return ResolveFlag(rUnit, RuleFlagId_t::Flight)
        || ResolveFlag(rUnit, RuleFlagId_t::IgnoreZoneOfControl);
}

bool IsLandUnit_(const Unit& rUnit)
{
    return !ResolveFlag(rUnit, RuleFlagId_t::Flight) && !ResolveFlag(rUnit, RuleFlagId_t::Sea);
}

} // namespace

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)
{
    if (rProjector.GetFaction().GetFactionId() == rSubject.GetFaction().GetFactionId())
    {
        return false;
    }
    if (IgnoresZoneOfControl_(rSubject))
    {
        return false;
    }

    if (ResolveFlag(rProjector, RuleFlagId_t::Flight))
    {
        // Flight exerts on land and sea units, not on other flight units.
        return !ResolveFlag(rSubject, RuleFlagId_t::Flight);
    }
    if (ResolveFlag(rProjector, RuleFlagId_t::Sea))
    {
        return ResolveFlag(rSubject, RuleFlagId_t::Sea);
    }
    return IsLandUnit_(rSubject);
}

bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    if (IgnoresZoneOfControl_(rMover))
    {
        return false;
    }

    bool bInZoc = false;
    ForEachTileInChebyshevRadius(rTile, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            if (bInZoc)
            {
                return;
            }
            for (Unit* pUnit : rWorldMap.GetUnitsOnTile(*pNeighbor))
            {
                if (pUnit && UnitExertsZocOn(*pUnit, rMover))
                {
                    bInZoc = true;
                    return;
                }
            }
        });
    return bInZoc;
}

bool HasHostileUnit(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
    const FactionId_t moverId = rMover.GetFaction().GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != moverId)
        {
            return true;
        }
    }
    return false;
}

bool IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                    const WorldMap& rWorldMap)
{
    if (IgnoresZoneOfControl_(rMover))
    {
        return false;
    }
    if (!IsTileInHostileZoc(rMover, rFrom, rWorldMap))
    {
        return false;
    }
    return IsTileInHostileZoc(rMover, rTo, rWorldMap);
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile)
{
    if (ResolveFlag(rMover, RuleFlagId_t::Flight))
    {
        return true;
    }
    if (ResolveFlag(rMover, RuleFlagId_t::Sea))
    {
        return rTile.IsWater();
    }
    return rTile.IsLand();
}

bool CanStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap)
{
    if (rMover.GetMovesRemaining() <= 0)
    {
        return false;
    }
    if (!AreChebyshevAdjacent(rMover.GetTile(), rTo))
    {
        return false;
    }
    if (!CanEnterTileTerrain(rMover, rTo))
    {
        return false;
    }
    // Hostile units never share a tile; combat is resolved from an adjacent tile.
    if (HasHostileUnit(rMover, rTo, rWorldMap))
    {
        return false;
    }
    if (IsZocViolation(rMover, rMover.GetTile(), rTo, rWorldMap))
    {
        return false;
    }
    if (UnitPositionIndex::IsSingleUnitPerTile() && !rWorldMap.GetUnitsOnTile(rTo).empty())
    {
        return false;
    }

    return true;
}

void ResolveCombatStub(Unit& rAttacker)
{
    rAttacker.SetMovesRemaining(0);
    rAttacker.ClearOrder();
}

bool TryAttack(Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap)
{
    if (rAttacker.GetMovesRemaining() <= 0)
    {
        return false;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile))
    {
        return false;
    }
    if (!HasHostileUnit(rAttacker, rTargetTile, rWorldMap))
    {
        return false;
    }

    ResolveCombatStub(rAttacker);
    return true;
}

bool TryStep(Unit& rMover, const Tile& rTo, WorldMap& rWorldMap)
{
    if (!CanStep(rMover, rTo, rWorldMap))
    {
        return false;
    }

    if (!rWorldMap.GetUnitPositions().TryMoveUnit(rMover, rTo))
    {
        return false;
    }
    rMover.SetMovesRemaining(rMover.GetMovesRemaining() - 1);
    return true;
}

const Tile* ProposeNextStep(const Unit& rMover, const Tile& rDestination,
                            const WorldMap& rWorldMap)
{
    const Tile& rFrom = rMover.GetTile();
    if (&rFrom == &rDestination)
    {
        return nullptr;
    }

    const int currentDist = ChebyshevDistance(rFrom, rDestination);
    const Tile* pBest = nullptr;
    int bestDist = currentDist;

    ForEachTileInChebyshevRadius(rFrom, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            if (!CanStep(rMover, *pTile, rWorldMap))
            {
                return;
            }
            const int dist = ChebyshevDistance(*pTile, rDestination);
            if (dist < bestDist)
            {
                bestDist = dist;
                pBest = pTile;
            }
        });

    return pBest;
}

} // namespace ac
