#include "game/units/MovementRules.h"

#include "game/Faction.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementConstants.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentConfig.h"

namespace ac
{

namespace
{

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

// True when rRemaining fragments can pay rCost. Cost 0 (mag tubes) only needs any moves
// left. Costs above remaining are still allowed when at least one full move point remains
// (SMAC: rocky/fungus always enterable with >= 1 MP left; excess cost zeroes the unit).
bool CanAffordMoveCost_(int remaining, int cost)
{
    if (remaining <= 0)
    {
        return false;
    }
    if (cost <= remaining)
    {
        return true;
    }
    return remaining >= MovementConstants_t::k_moveFragmentsPerPoint;
}

} // namespace

bool UnitExertsZocOn(const Unit& rProjector, const Unit& rSubject)
{
    if (rProjector.GetFaction().GetFactionId() == rSubject.GetFaction().GetFactionId())
    {
        return false;
    }
    // Air subjects are already excluded by the domain match below; this flag is for
    // land/sea units (e.g. probes) that ignore ZOC without changing domain.
    if (ResolveFlag(rSubject, RuleFlagId_t::IgnoreZoneOfControl))
    {
        return false;
    }

    switch (rProjector.GetDomain())
    {
    case UnitDomain_t::Air:
        // Air exerts on land and sea units, not on other air units.
        return rSubject.GetDomain() != UnitDomain_t::Air;
    case UnitDomain_t::Sea:
        return rSubject.GetDomain() == UnitDomain_t::Sea;
    case UnitDomain_t::Land:
        return rSubject.GetDomain() == UnitDomain_t::Land;
    }
    return false;
}

bool IsTileInHostileZoc(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap)
{
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
    if (!IsTileInHostileZoc(rMover, rFrom, rWorldMap))
    {
        return false;
    }
    return IsTileInHostileZoc(rMover, rTo, rWorldMap);
}

bool CanEnterTileTerrain(const Unit& rMover, const Tile& rTile)
{
    switch (rMover.GetDomain())
    {
    case UnitDomain_t::Air:
        return true;
    case UnitDomain_t::Sea:
        return rTile.IsWater();
    case UnitDomain_t::Land:
        return rTile.IsLand();
    }
    return false;
}

StepEvaluation_t EvaluateStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap,
                              const ImprovementRegistry& rImprovements)
{
    StepEvaluation_t result;

    const MoveCostCalculator calc(rImprovements);
    const int cost = calc.ComputeFragments(rTo, MoveProfileFor(rMover));
    if (!CanAffordMoveCost_(rMover.GetMovesRemaining(), cost))
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

bool CanStep(const Unit& rMover, const Tile& rTo, const WorldMap& rWorldMap,
             const ImprovementRegistry& rImprovements)
{
    return EvaluateStep(rMover, rTo, rWorldMap, rImprovements).outcome == StepOutcome_t::Legal;
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

bool TryStep(Unit& rMover, const Tile& rTo, WorldMap& rWorldMap,
             const ImprovementRegistry& rImprovements)
{
    const StepEvaluation_t eval = EvaluateStep(rMover, rTo, rWorldMap, rImprovements);
    if (eval.outcome == StepOutcome_t::Legal)
    {
        if (!rWorldMap.GetUnitPositions().TryMoveUnit(rMover, rTo))
        {
            return false;
        }
        const MoveCostCalculator calc(rImprovements);
        const int cost = calc.ComputeFragments(rTo, MoveProfileFor(rMover));
        rMover.SetMovesRemaining(rMover.GetMovesRemaining() - cost);
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
                            const WorldMap& rWorldMap,
                            const ImprovementRegistry& rImprovements)
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
            if (!CanStep(rMover, *pTile, rWorldMap, rImprovements))
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
                               const WorldMap& rWorldMap,
                               const ImprovementRegistry& rImprovements)
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
            const StepEvaluation_t eval = EvaluateStep(rMover, *pTile, rWorldMap, rImprovements);
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
