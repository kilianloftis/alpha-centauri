#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

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

void CollectHostileOccupants_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                              std::vector<Unit*>& rOut)
{
    const FactionId_t moverId = rMover.GetFaction().GetFactionId();
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != moverId)
        {
            rOut.push_back(pUnit);
        }
    }
}

void CollectZocProjectorsAround_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                                 std::vector<Unit*>& rOut)
{
    ForEachTileInChebyshevRadius(rTile, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            for (Unit* pUnit : rWorldMap.GetUnitsOnTile(*pNeighbor))
            {
                if (pUnit && UnitExertsZocOn(*pUnit, rMover))
                {
                    rOut.push_back(pUnit);
                }
            }
        });
}

void RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval)
{
    FactionRevealedUnits& rRevealed = rMover.GetFaction().GetRevealedUnits();
    for (Unit* pUnit : rEval.blockingUnits)
    {
        if (pUnit)
        {
            rRevealed.Reveal(*pUnit);
        }
    }
}

bool IsDesiredStepCandidate_(StepOutcome_t outcome)
{
    return outcome == StepOutcome_t::Legal
        || outcome == StepOutcome_t::BlockedByOccupant
        || outcome == StepOutcome_t::BlockedByZoc;
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

bool HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                           const TileEffectsContext& rTileEffects)
{
    const Faction& rObserver = rMover.GetFaction();
    const FactionId_t moverId = rObserver.GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != moverId
            && IsUnitVisibleTo(rObserver, *pUnit, rTileEffects))
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

StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap)
{
    StepEvaluation_t result;

    if (rMover.GetMovesRemaining() <= 0)
    {
        result.outcome = StepOutcome_t::NoMoves;
        return result;
    }
    if (!AreChebyshevAdjacent(rMover.GetTile(), rTo))
    {
        result.outcome = StepOutcome_t::NotAdjacent;
        return result;
    }
    if (!CanEnterTileTerrain(rMover, rTo))
    {
        result.outcome = StepOutcome_t::BlockedByTerrain;
        return result;
    }

    // Hostile units never share a tile; combat is resolved from an adjacent tile.
    CollectHostileOccupants_(rMover, rTo, rWorldMap, result.blockingUnits);
    if (!result.blockingUnits.empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        return result;
    }

    if (IsZocViolation(rMover, rMover.GetTile(), rTo, rWorldMap))
    {
        result.outcome = StepOutcome_t::BlockedByZoc;
        CollectZocProjectorsAround_(rMover, rMover.GetTile(), rWorldMap, result.blockingUnits);
        CollectZocProjectorsAround_(rMover, rTo, rWorldMap, result.blockingUnits);
        return result;
    }

    if (UnitPositionIndex::IsSingleUnitPerTile() && !rWorldMap.GetUnitsOnTile(rTo).empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTo))
        {
            if (pUnit)
            {
                result.blockingUnits.push_back(pUnit);
            }
        }
        return result;
    }

    result.outcome = StepOutcome_t::Legal;
    return result;
}

bool CanStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap)
{
    return EvaluateStep(rMover, rTo, rWorldMap).outcome == StepOutcome_t::Legal;
}

void ResolveCombatStub(Unit& rAttacker)
{
    rAttacker.SetMovesRemaining(0);
    rAttacker.ClearOrder();
}

bool TryAttack(Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap,
               const TileEffectsContext& rTileEffects)
{
    if (rAttacker.GetMovesRemaining() <= 0)
    {
        return false;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile))
    {
        return false;
    }
    if (!HasVisibleHostileUnit(rAttacker, rTargetTile, rWorldMap, rTileEffects))
    {
        return false;
    }

    ResolveCombatStub(rAttacker);
    return true;
}

bool TryStep(Unit& rMover, const Tile& rTo, WorldMap& rWorldMap)
{
    const StepEvaluation_t eval = EvaluateStep(rMover, rTo, rWorldMap);
    if (eval.outcome == StepOutcome_t::Legal)
    {
        if (!rWorldMap.GetUnitPositions().TryMoveUnit(rMover, rTo))
        {
            return false;
        }
        rMover.SetMovesRemaining(rMover.GetMovesRemaining() - 1);
        return true;
    }

    if (eval.outcome == StepOutcome_t::BlockedByOccupant
        || eval.outcome == StepOutcome_t::BlockedByZoc)
    {
        RevealBlockingUnits_(rMover, eval);
    }
    return false;
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

const Tile* ProposeDesiredStep(const Unit& rMover, const Tile& rDestination,
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
            const StepEvaluation_t eval = EvaluateStep(rMover, *pTile, rWorldMap);
            if (!IsDesiredStepCandidate_(eval.outcome))
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
