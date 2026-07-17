#include "game/units/StepEvaluator.h"

#include "game/Faction.h"
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

void CollectHostileOccupants_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                              std::vector<Unit*>& rOut)
{
    ForEachHostileOnTile_(rMover, rTile, rWorldMap, [&](Unit& rUnit)
    {
        rOut.push_back(&rUnit);
        return true;
    });
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

void CollectZocProjectorsAround_(const Unit& rMover, const Tile& rTile, const WorldMap& rWorldMap,
                                 std::vector<Unit*>& rOut)
{
    ForEachZocProjectorAround_(rMover, rTile, rWorldMap, [&](Unit& rUnit)
    {
        rOut.push_back(&rUnit);
        return true;
    });
}

} // namespace

StepEvaluator::StepEvaluator(WorldMap& rWorldMap, const TileEffectsContext& rTileEffects)
    : m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
{
}

bool StepEvaluator::IsTileInHostileZoc(const Unit& rMover, const Tile& rTile) const
{
    bool bInZoc = false;
    ForEachZocProjectorAround_(rMover, rTile, m_rWorldMap, [&](Unit& /*rProjector*/)
    {
        bInZoc = true;
        return false;
    });
    return bInZoc;
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

bool StepEvaluator::IsZocViolation(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    if (!IsTileInHostileZoc(rMover, rFrom))
    {
        return false;
    }
    return IsTileInHostileZoc(rMover, rTo);
}

StepEvaluation_t StepEvaluator::EvaluateStep(const Unit& rMover, const Tile& rFrom,
                                             const Tile& rTo) const
{
    StepEvaluation_t result;

    if (!AreChebyshevAdjacent(rFrom, rTo))
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

    if (IsZocViolation(rMover, rFrom, rTo))
    {
        result.outcome = StepOutcome_t::BlockedByZoc;
        CollectZocProjectorsAround_(rMover, rFrom, m_rWorldMap, result.blockingUnits);
        CollectZocProjectorsAround_(rMover, rTo, m_rWorldMap, result.blockingUnits);
        return result;
    }

    if (!CanPlaceUnitOnTile(rTo, m_rWorldMap.GetUnitPositions()))
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

bool StepEvaluator::CanStep(const Unit& rMover, const Tile& rFrom, const Tile& rTo) const
{
    return EvaluateStep(rMover, rFrom, rTo).outcome == StepOutcome_t::Legal;
}

} // namespace ac
