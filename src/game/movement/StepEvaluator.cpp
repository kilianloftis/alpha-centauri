#include "game/movement/StepEvaluator.h"

#include "game/faction/Faction.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/VisibilityRules.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/units/Unit.h"
#include "game/effects/TileEffectsContext.h"

namespace ac
{

namespace
{

bool IsHostileTo_(const Unit& rMover, const Unit& rOther)
{
    return !MayShareTiles(rMover.GetFaction(), rOther.GetFaction().GetFactionId());
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
    // Embarked cargo is hostile only in a base (same eligibility as combat defense).
    if (!rTile.HasImprovement(ImprovementIds::k_Base))
    {
        return;
    }
    for (Unit* pUnit : rWorldMap.GetCargoOnTile(rTile))
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
    ForEachNeighborInChebyshevRadius(rTile, rWorldMap, /*radius=*/1,
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
    return IsUnitVisibleTo(rMover.GetFaction(), rOther);
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
    return CanPhysicallyEnterTile(rMover, rTile);
}

bool StepEvaluator::IsForbiddenTerritory(const Unit& rMover, const Tile& rTile) const
{
    return !MayEnterTerritoryOf(rMover, m_rWorldMap.GetTerritory().GetOwner(rTile));
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

bool StepEvaluator::IsZocViolation_(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                                    Knowledge_t knowledge) const
{
    if (!IsTileInHostileZoc_(rMover, rFrom, knowledge))
    {
        return false;
    }
    // SMAC: ZOC→ZOC is legal when the destination already holds a friendly unit or base.
    if (HasFriendlyOccupant(rMover, rTo) || HasFriendlyBase(rMover, rTo))
    {
        return false;
    }
    return IsTileInHostileZoc_(rMover, rTo, knowledge);
}

StepEvaluation_t StepEvaluator::EvaluateStep_(const Unit& rMover, const Tile& rFrom,
                                              const Tile& rTo, Knowledge_t knowledge) const
{
    StepEvaluation_t result;
    if (!AreChebyshevAdjacent(rFrom, rTo, m_rWorldMap.GetWidth()))
    {
        result.outcome = StepOutcome_t::NotAdjacent;
        return result;
    }
    if (!TerrainAllowsStep_(rMover, rFrom, rTo, knowledge))
    {
        result.outcome = StepOutcome_t::BlockedByTerrain;
        return result;
    }
    if (knowledge == Knowledge_t::Objective && IsForbiddenTerritory(rMover, rTo))
    {
        result.outcome = StepOutcome_t::BlockedByTerritory;
        return result;
    }
    // Hostile units never share a tile; combat is resolved from an adjacent tile.
    result.blockingUnits = KnownHostilesOn_(rMover, rTo, knowledge);
    if (!result.blockingUnits.empty())
    {
        result.outcome = StepOutcome_t::BlockedByOccupant;
        return result;
    }
    if (IsZocViolation_(rMover, rFrom, rTo, knowledge))
    {
        result.outcome = StepOutcome_t::BlockedByZoc;
        result.blockingUnits = KnownZocProjectors_(rMover, rFrom, rTo, knowledge);
        return result;
    }
    result.outcome = StepOutcome_t::Legal;
    return result;
}

// Embarked units may only unload to an adjacent legal tile.
bool StepEvaluator::TerrainAllowsStep_(const Unit& rMover, const Tile& rFrom, const Tile& rTo,
                                       Knowledge_t knowledge) const
{
    if (rMover.IsEmbarked())
    {
        return CanUnloadTo(rMover, rFrom, rTo);
    }
    return CanEnterTerrain_(rMover, rTo, knowledge);
}

std::vector<Unit*> StepEvaluator::KnownHostilesOn_(const Unit& rMover, const Tile& rTile,
                                                   Knowledge_t knowledge) const
{
    std::vector<Unit*> hostiles;
    ForEachHostileOnTile_(rMover, rTile, m_rWorldMap, [&](Unit& rHostile)
    {
        if (UnitCountsForPlanner_(rMover, rHostile, knowledge))
        {
            hostiles.push_back(&rHostile);
        }
        return true;
    });
    return hostiles;
}

std::vector<Unit*> StepEvaluator::KnownZocProjectors_(const Unit& rMover, const Tile& rFrom,
                                                      const Tile& rTo, Knowledge_t knowledge) const
{
    std::vector<Unit*> projectors;
    for (const Tile* pTile : {&rFrom, &rTo})
    {
        ForEachZocProjectorAround_(rMover, *pTile, m_rWorldMap, [&](Unit& rProjector)
        {
            if (UnitCountsForPlanner_(rMover, rProjector, knowledge))
            {
                projectors.push_back(&rProjector);
            }
            return true;
        });
    }
    return projectors;
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

bool StepEvaluator::CanPlanEnterTerrain(const Unit& rMover, const Tile& rTile) const
{
    return CanEnterTerrain_(rMover, rTile, Knowledge_t::FactionKnown);
}

} // namespace ac
