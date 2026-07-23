#include "game/units/UnitOrderExecutor.h"

#include "game/units/UnitOrder.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/Pathfinder.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/faction/UnitVisibility.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/Faction.h"

#include <stdexcept>
#include <variant>

namespace ac
{

UnitOrderExecutor::UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                                     const StepEvaluator& rSteps,
                                     WorldMap& rWorldMap,
                                     const TileEffectsContext& rTileEffects,
                                     Pathfinder& rPathfinder)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects)
{
}

UnitOrderExecutor::UnitOrderExecutor(const MoveCostCalculator& rMoveCosts,
                                     const StepEvaluator& rSteps,
                                     WorldMap& rWorldMap,
                                     const TileEffectsContext& rTileEffects,
                                     Pathfinder& rPathfinder,
                                     uint32_t combatSeed)
    : m_rMoveCosts(rMoveCosts)
    , m_rSteps(rSteps)
    , m_rWorldMap(rWorldMap)
    , m_rTileEffects(rTileEffects)
    , m_rPathfinder(rPathfinder)
    , m_combat(rMoveCosts, rSteps, rWorldMap, rTileEffects, combatSeed)
{
}

void UnitOrderExecutor::RevealBlockingUnits_(Unit& rMover, const StepEvaluation_t& rEval)
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

void UnitOrderExecutor::CollectVisibleHostileIds_(const Unit& rObserver,
                                                  std::unordered_set<UnitId_t>& rOut) const
{
    const Faction& rFaction = rObserver.GetFaction();
    const FactionId_t observerId = rFaction.GetFactionId();

    for (const auto& pTile : m_rWorldMap.GetTiles())
    {
        if (!pTile)
        {
            continue;
        }
        for (const Unit* pUnit : m_rWorldMap.GetUnitsOnTile(*pTile))
        {
            if (pUnit && pUnit->GetFaction().GetFactionId() != observerId
                && IsUnitVisibleTo(rFaction, *pUnit, m_rTileEffects))
            {
                rOut.insert(pUnit->GetUnitId());
            }
        }
    }
}

bool UnitOrderExecutor::HasNewlyVisibleHostile_(
    const Unit& rObserver, const std::unordered_set<UnitId_t>& rPreviouslyVisible) const
{
    std::unordered_set<UnitId_t> nowVisible;
    CollectVisibleHostileIds_(rObserver, nowVisible);
    for (UnitId_t id : nowVisible)
    {
        if (!rPreviouslyVisible.contains(id))
        {
            return true;
        }
    }
    return false;
}

void UnitOrderExecutor::CancelMoveOrderIfNewHostile_(
    Unit& rMover, const std::unordered_set<UnitId_t>& rPreviouslyVisible)
{
    if (!rMover.GetOrder().has_value()
        || !std::holds_alternative<MoveOrder_t>(*rMover.GetOrder()))
    {
        return;
    }
    if (HasNewlyVisibleHostile_(rMover, rPreviouslyVisible))
    {
        rMover.ClearOrder();
    }
}

Unit* UnitOrderExecutor::FindVisibleHostileOnTile_(const Unit& rAttacker,
                                                   const Tile& rTile) const
{
    const Faction& rObserver = rAttacker.GetFaction();
    const FactionId_t observerId = rObserver.GetFactionId();
    for (Unit* pUnit : m_rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != observerId
            && IsUnitVisibleTo(rObserver, *pUnit, m_rTileEffects))
        {
            return pUnit;
        }
    }
    return nullptr;
}

void UnitOrderExecutor::EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter)
{
    m_rWorldMap.GetUnitPositions().MoveUnit(rMover, rTo);
    rMover.SetMoveFragmentsRemaining(remainingAfter);
}

bool UnitOrderExecutor::SpendMovesAndEnter_(Unit& rMover, const Tile& rTo,
                                            MoveOrder_t& rMoveOrder)
{
    const EntryTerms_t terms = m_rMoveCosts.ForUnit(rMover, m_rWorldMap).EntryTerms(rTo);
    const int available = rMover.GetMoveFragmentsRemaining();

    if (terms.bRequiresFullCost)
    {
        if (rMoveOrder.pChargeTile != &rTo)
        {
            rMoveOrder.pChargeTile = &rTo;
            rMoveOrder.chargeFragmentsPaid = 0;
        }
        if (rMoveOrder.chargeFragmentsPaid + available < terms.costFragments)
        {
            // Bank this turn's fragments toward the entry price and stay put.
            rMoveOrder.chargeFragmentsPaid += available;
            rMover.SetMoveFragmentsRemaining(0);
            return false;
        }
    }

    rMoveOrder.pChargeTile = nullptr;
    rMoveOrder.chargeFragmentsPaid = 0;

    // Standard terrain admits any positive balance (SetMoveFragmentsRemaining clamps a
    // negative result to 0); end-turn entries wipe whatever would remain.
    const int remainingAfter = terms.bEndsTurn ? 0 : available - terms.costFragments;
    EnterTile_(rMover, rTo, remainingAfter);
    return true;
}

bool UnitOrderExecutor::TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder)
{
    if (rMover.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }

    std::unordered_set<UnitId_t> visibleBefore;
    CollectVisibleHostileIds_(rMover, visibleBefore);

    const StepEvaluation_t eval = m_rSteps.EvaluateStep(rMover, rMover.GetTile(), rTo);
    if (eval.outcome == StepOutcome_t::Legal)
    {
        const bool bEntered = SpendMovesAndEnter_(rMover, rTo, rMoveOrder);
        if (bEntered)
        {
            CancelMoveOrderIfNewHostile_(rMover, visibleBefore);
        }
        return bEntered;
    }

    if (eval.outcome == StepOutcome_t::BlockedByOccupant
        || eval.outcome == StepOutcome_t::BlockedByZoc)
    {
        RevealBlockingUnits_(rMover, eval);
        CancelMoveOrderIfNewHostile_(rMover, visibleBefore);
    }
    return false;
}

std::optional<CombatResult_t> UnitOrderExecutor::TryAttack(Unit& rAttacker,
                                                           const Tile& rTargetTile)
{
    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return std::nullopt;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile, m_rWorldMap.GetWidth()))
    {
        return std::nullopt;
    }

    Unit* pDefender = FindVisibleHostileOnTile_(rAttacker, rTargetTile);
    if (!pDefender)
    {
        return std::nullopt;
    }

    CombatResult_t result = m_combat.Resolve(rAttacker, *pDefender);
    if (!result.bAttackerDestroyed)
    {
        rAttacker.MarkAttacked();
        rAttacker.SetMoveFragmentsRemaining(0);
        rAttacker.ClearOrder();
    }
    return result;
}

void UnitOrderExecutor::Execute(Unit& rUnit)
{
    if (!rUnit.GetOrder().has_value())
        return;

    std::visit([&](auto& rOrder)
    {
        Execute_(rUnit, rOrder);
    }, *rUnit.GetOrder()); // non-const overload — allows mutating HoldForTurnsOrder_t
}

void UnitOrderExecutor::Execute_(Unit& rUnit, MoveOrder_t& rOrder)
{
    if (!rOrder.pDestination)
        throw std::runtime_error("MoveOrder has null destination");

    // Snapshot: TryStep may ClearOrder on new hostiles, invalidating rOrder.
    const Tile* const pDestination = rOrder.pDestination;

    while (true)
    {
        if (&rUnit.GetTile() == pDestination)
        {
            rUnit.ClearOrder();
            return;
        }

        if (rUnit.GetMoveFragmentsRemaining() <= 0)
        {
            return;
        }

        // Re-resolve each step: rOrder from visit is dangling after ClearOrder.
        MoveOrder_t& rLiveOrder = std::get<MoveOrder_t>(*rUnit.GetOrder());

        // Path is recalculated every step so newly revealed fog / hostiles are accounted for.
        const Tile* pNext = m_rPathfinder.NextStep(rUnit, *pDestination);
        if (!pNext)
        {
            const Tile* pDesired = m_rPathfinder.DesiredContactStep(rUnit, *pDestination);
            if (pDesired)
            {
                TryStep(rUnit, *pDesired, rLiveOrder);
            }
            return;
        }

        const Tile* pTileBefore = &rUnit.GetTile();
        const int movesBefore = rUnit.GetMoveFragmentsRemaining();

        if (!TryStep(rUnit, *pNext, rLiveOrder))
        {
            return;
        }

        if (&rUnit.GetTile() == pTileBefore && rUnit.GetMoveFragmentsRemaining() == movesBefore)
        {
            return;
        }

        if (!rUnit.GetOrder().has_value())
        {
            return;
        }
    }
}

void UnitOrderExecutor::Execute_(Unit& rUnit, HoldOrder_t& rOrder)
{
    // Hold indefinitely — nothing to do each turn
    (void)rUnit;
    (void)rOrder;
}

void UnitOrderExecutor::Execute_(Unit& rUnit, HoldUntilHealedOrder_t& rOrder)
{
    // TODO: Clear order when unit reaches full HP
    (void)rUnit;
    (void)rOrder;
}

void UnitOrderExecutor::Execute_(Unit& rUnit, HoldForTurnsOrder_t& rOrder)
{
    if (rOrder.turnsRemaining <= 0)
    {
        rUnit.ClearOrder();
        return;
    }

    --rOrder.turnsRemaining;
    if (rOrder.turnsRemaining == 0)
        rUnit.ClearOrder();
}

void UnitOrderExecutor::Execute_(Unit& rUnit, SupplyCrawlOrder_t& rOrder)
{
    // Harvest happens in ResourceCollection via HomeBaseIndex units in ResourceManager.
    (void)rUnit;
    (void)rOrder;
}

} // namespace ac
