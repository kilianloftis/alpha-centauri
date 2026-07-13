#include "game/units/StepEvaluator.h"

#include "game/Faction.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementConstants.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"

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

StepEvaluator::StepEvaluator(const ImprovementRegistry& rImprovements,
                             WorldMap& rWorldMap,
                             const TileEffectsContext& rTileEffects)
    : m_moveCosts(rImprovements)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
{
}

WorldMap& StepEvaluator::GetWorldMap() const
{
    return m_rWorldMap;
}

const MoveCostCalculator& StepEvaluator::GetMoveCosts() const
{
    return m_moveCosts;
}

bool StepEvaluator::IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const
{
    bool bInZoc = false;
    ForEachTileInChebyshevRadius(rTile, m_rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            if (bInZoc)
            {
                return;
            }
            for (Unit* pUnit : m_rWorldMap.GetUnitsOnTile(*pNeighbor))
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

bool StepEvaluator::HasHostileUnit(const Unit& rMover, const Tile& rTile) const
{
    const FactionId_t moverId = rMover.GetFaction().GetFactionId();
    for (const Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != moverId)
        {
            return true;
        }
    }
    return false;
}

bool StepEvaluator::HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile) const
{
    const Faction& rObserver = rMover.GetFaction();
    const FactionId_t moverId = rObserver.GetFactionId();
    for (const Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != moverId
            && IsUnitVisibleTo(rObserver, *pUnit, m_rTileEffects))
        {
            return true;
        }
    }
    return false;
}

bool StepEvaluator::IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    if (!IsTileInHostileZoc(rMover, rFrom))
    {
        return false;
    }
    return IsTileInHostileZoc(rMover, rTo);
}

StepEvaluation_t StepEvaluator::EvaluateStep(const Unit& rMover, const Tile& rTo) const
{
    StepEvaluation_t result;

    const int cost = m_moveCosts.ComputeFragments(rTo, MoveProfileFor(rMover));
    if (!CanAffordMoveCost_(rMover.GetMoveFragmentsRemaining(), cost))
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
    CollectHostileOccupants_(rMover, rTo, m_rWorldMap, result.blockingUnits);
    if (!result.blockingUnits.empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        return result;
    }

    if (IsZocViolation(rMover, rMover.GetTile(), rTo))
    {
        result.outcome = StepOutcome_t::BlockedByZoc;
        CollectZocProjectorsAround_(rMover, rMover.GetTile(), m_rWorldMap, result.blockingUnits);
        CollectZocProjectorsAround_(rMover, rTo, m_rWorldMap, result.blockingUnits);
        return result;
    }

    if (UnitPositionIndex::IsSingleUnitPerTile() && !m_rWorldMap.GetUnitsOnTile(rTo).empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        for (Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTo))
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

bool StepEvaluator::CanStep(const Unit& rMover, const Tile& rTo) const
{
    return EvaluateStep(rMover, rTo).outcome == StepOutcome_t::Legal;
}

const Tile* StepEvaluator::ProposeNextStep(const Unit& rMover, const Tile& rDestination) const
{
    const Tile& rFrom = rMover.GetTile();
    if (&rFrom == &rDestination)
    {
        return nullptr;
    }

    const int currentDist = ChebyshevDistance(rFrom, rDestination);
    const Tile* pBest = nullptr;
    int bestDist = currentDist;

    ForEachTileInChebyshevRadius(rFrom, m_rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            if (!CanStep(rMover, *pTile))
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

const Tile* StepEvaluator::ProposeDesiredStep(const Unit& rMover, const Tile& rDestination) const
{
    const Tile& rFrom = rMover.GetTile();
    if (&rFrom == &rDestination)
    {
        return nullptr;
    }

    const int currentDist = ChebyshevDistance(rFrom, rDestination);
    const Tile* pBest = nullptr;
    int bestDist = currentDist;

    ForEachTileInChebyshevRadius(rFrom, m_rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            const StepEvaluation_t eval = EvaluateStep(rMover, *pTile);
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
