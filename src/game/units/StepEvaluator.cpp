#include "game/units/StepEvaluator.h"

#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"

namespace ac
{

namespace
{

bool IsHostileTo_(const Unit& rMover, const Unit& rOther)
{
    return rOther.GetFaction().GetFactionId() != rMover.GetFaction().GetFactionId();
}

// Invokes rFn(Unit&) for each hostile unit on rTile; stops early if rFn returns false.
template <typename Fn>
void ForEachHostileOnTile_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                           Fn&& rFn)
{
    for (Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && IsHostileTo_(rMover, *pUnit) && !rFn(*pUnit))
        {
            return;
        }
    }
}

// Invokes rFn(Unit&) for each ZOC projector around rTile; stops early if rFn returns false.
template <typename Fn>
void ForEachZocProjectorAround_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                                Fn&& rFn)
{
    ForEachTileInChebyshevRadius(rTile, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pNeighbor, int /*distance*/)
        {
            for (Unit* pUnit : rWorldMap.GetUnitsOnTile(*pNeighbor))
            {
                if (pUnit && UnitExertsZocOn(*pUnit, rMover) && !rFn(*pUnit))
                {
                    return;
                }
            }
        });
}

} // namespace

StepEvaluator::StepEvaluator(WorldMap& rWorldMap, const TileEffectsContext& rTileEffects)
    : m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
{
}

bool StepEvaluator::UnitCountsForPlanner_(const Unit& rMover, const Unit& rOther,
                                          Knowledge_t knowledge) const
{
    if (knowledge == Knowledge_t::Objective)
    {
        return true;
    }
    return IsUnitVisibleTo(rMover.GetFaction(), rOther, m_rTileEffects);
}

bool StepEvaluator::CanEnterTerrain_(const Unit& rMover, const Tile& rTile,
                                     Knowledge_t knowledge) const
{
    if (knowledge == Knowledge_t::FactionKnown)
    {
        const FactionExploredMap& rExplored = rMover.GetFaction().GetExploredMap();
        if (rExplored.IsSized() && !rExplored.IsExplored(rTile))
        {
            // Shroud: assume domain-compatible until the tile is explored.
            return true;
        }
    }
    return CanEnterTileTerrain(rMover, rTile);
}

bool StepEvaluator::IsTileInHostileZoc_(const Unit& rMover, const Tile& rTile,
                                        Knowledge_t knowledge) const
{
    bool bInZoc = false;
    ForEachZocProjectorAround_(rMover, rTile, m_rWorldMap, [&](Unit& rProjector)
    {
        if (!UnitCountsForPlanner_(rMover, rProjector, knowledge))
        {
            return true;
        }
        bInZoc = true;
        return false;
    });
    return bInZoc;
}

bool StepEvaluator::IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const
{
    return IsTileInHostileZoc_(rMover, rTile, Knowledge_t::Objective);
}

bool StepEvaluator::HasHostileUnit(const Unit& rMover, const Tile& rTile) const
{
    bool bFound = false;
    ForEachHostileOnTile_(rMover, rTile, m_rWorldMap, [&](Unit& /*rHostile*/)
    {
        bFound = true;
        return false;
    });
    return bFound;
}

bool StepEvaluator::HasVisibleHostileUnit(const Unit& rMover, const Tile& rTile) const
{
    const Faction& rObserver = rMover.GetFaction();
    bool bFound = false;
    ForEachHostileOnTile_(rMover, rTile, m_rWorldMap, [&](Unit& rHostile)
    {
        if (IsUnitVisibleTo(rObserver, rHostile, m_rTileEffects))
        {
            bFound = true;
            return false;
        }
        return true;
    });
    return bFound;
}

bool StepEvaluator::IsZocViolation_(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                                    Knowledge_t knowledge) const
{
    if (!IsTileInHostileZoc_(rMover, rFrom, knowledge))
    {
        return false;
    }
    // SMAC: ZOC→ZOC is legal when the destination already holds a friendly unit or base.
    if (HasFriendlyOccupant(rMover, rTo, m_rWorldMap) || HasFriendlyBase(rMover, rTo))
    {
        return false;
    }
    return IsTileInHostileZoc_(rMover, rTo, knowledge);
}

bool StepEvaluator::IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    return IsZocViolation_(rMover, rFrom, rTo, Knowledge_t::Objective);
}

StepEvaluation_t StepEvaluator::EvaluateStep_(const Unit& rMover, const Tile& rFrom,
                                              const Tile& rTo, Knowledge_t knowledge) const
{
    StepEvaluation_t result;

    if (!AreChebyshevAdjacent(rFrom, rTo))
    {
        result.outcome = StepOutcome_t::NotAdjacent;
        return result;
    }
    if (!CanEnterTerrain_(rMover, rTo, knowledge))
    {
        result.outcome = StepOutcome_t::BlockedByTerrain;
        return result;
    }

    // Hostile units never share a tile; combat is resolved from an adjacent tile.
    ForEachHostileOnTile_(rMover, rTo, m_rWorldMap, [&](Unit& rHostile)
    {
        if (UnitCountsForPlanner_(rMover, rHostile, knowledge))
        {
            result.blockingUnits.push_back(&rHostile);
        }
        return true;
    });
    if (!result.blockingUnits.empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        return result;
    }

    if (IsZocViolation_(rMover, rFrom, rTo, knowledge))
    {
        result.outcome = StepOutcome_t::BlockedByZoc;
        ForEachZocProjectorAround_(rMover, rFrom, m_rWorldMap, [&](Unit& rProjector)
        {
            if (UnitCountsForPlanner_(rMover, rProjector, knowledge))
            {
                result.blockingUnits.push_back(&rProjector);
            }
            return true;
        });
        ForEachZocProjectorAround_(rMover, rTo, m_rWorldMap, [&](Unit& rProjector)
        {
            if (UnitCountsForPlanner_(rMover, rProjector, knowledge))
            {
                result.blockingUnits.push_back(&rProjector);
            }
            return true;
        });
        return result;
    }

    if (IsSingleUnitPerTile())
    {
        for (Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTo))
        {
            if (pUnit && UnitCountsForPlanner_(rMover, *pUnit, knowledge))
            {
                result.blockingUnits.push_back(pUnit);
            }
        }
        if (!result.blockingUnits.empty())
        {
            result.outcome = StepOutcome_t::BlockedByOccupant;
            return result;
        }
    }

    result.outcome = StepOutcome_t::Legal;
    return result;
}

StepEvaluation_t StepEvaluator::EvaluateStep(const Unit& rMover, const Tile& rFrom,
                                             const Tile& rTo) const
{
    return EvaluateStep_(rMover, rFrom, rTo, Knowledge_t::Objective);
}

bool StepEvaluator::CanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    return EvaluateStep(rMover, rFrom, rTo).outcome == StepOutcome_t::Legal;
}

StepEvaluation_t StepEvaluator::EvaluatePlannedStep(const Unit& rMover, const Tile& rFrom,
                                                    const Tile& rTo) const
{
    return EvaluateStep_(rMover, rFrom, rTo, Knowledge_t::FactionKnown);
}

bool StepEvaluator::CanPlanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    return EvaluatePlannedStep(rMover, rFrom, rTo).outcome == StepOutcome_t::Legal;
}

} // namespace ac
