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

void UnitOrderExecutor::ResolveCombatStub_(Unit& rAttacker)
{
    rAttacker.SetMoveFragmentsRemaining(0);
    rAttacker.ClearOrder();
}

void UnitOrderExecutor::EnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter)
{
    m_rWorldMap.GetUnitPositions().MoveUnit(rMover, rTo);
    rMover.SetMoveFragmentsRemaining(remainingAfter);
}

bool UnitOrderExecutor::SpendMovesAndEnter_(Unit& rMover, const Tile& rTo,
                                            MoveOrder_t& rMoveOrder)
{
    const auto costs = m_rMoveCosts.ForUnit(rMover, m_rWorldMap);
    const int available = rMover.GetMoveFragmentsRemaining();
    const int consumed = costs.FragmentsConsumed(rTo, available);

    if (available >= consumed)
    {
        rMoveOrder.pFungusChargeTile = nullptr;
        rMoveOrder.fungusFragmentsPaid = 0;
        EnterTile_(rMover, rTo, available - consumed);
        return true;
    }

    // Multi-turn charge: bank fragments across turns until full cost is paid.
    if (rMoveOrder.pFungusChargeTile != &rTo)
    {
        rMoveOrder.pFungusChargeTile = &rTo;
        rMoveOrder.fungusFragmentsPaid = 0;
    }
    rMoveOrder.fungusFragmentsPaid += available;

    if (rMoveOrder.fungusFragmentsPaid >= consumed)
    {
        rMoveOrder.pFungusChargeTile = nullptr;
        rMoveOrder.fungusFragmentsPaid = 0;
        EnterTile_(rMover, rTo, 0);
        return true;
    }

    rMover.SetMoveFragmentsRemaining(0);
    return false;
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

bool UnitOrderExecutor::TryAttack(Unit& rAttacker, const Tile& rTargetTile)
{
    if (rAttacker.GetMoveFragmentsRemaining() <= 0)
    {
        return false;
    }
    if (!AreChebyshevAdjacent(rAttacker.GetTile(), rTargetTile))
    {
        return false;
    }
    if (!m_rSteps.HasVisibleHostileUnit(rAttacker, rTargetTile))
    {
        return false;
    }

    ResolveCombatStub_(rAttacker);
    return true;
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

    if (&rUnit.GetTile() == rOrder.pDestination)
    {
        rUnit.ClearOrder();
        return;
    }

    if (rUnit.GetMoveFragmentsRemaining() <= 0)
    {
        // Keep the order for the next turn when moves refresh.
        return;
    }

    // Path is recalculated every step so newly revealed fog / hostiles are accounted for.
    const Tile* pNext = m_rPathfinder.NextStep(rUnit, *rOrder.pDestination);
    if (!pNext)
    {
        // No legal path — TryStep the desired bump tile so occupant/ZOC blockers are
        // contact-revealed (TryStep is a no-op move on those outcomes).
        const Tile* pDesired = m_rPathfinder.DesiredContactStep(rUnit, *rOrder.pDestination);
        if (pDesired)
        {
            TryStep(rUnit, *pDesired, rOrder);
        }
        return;
    }

    if (!TryStep(rUnit, *pNext, rOrder))
    {
        return;
    }

    // Combat stub / hostile reveal clears the order; otherwise clear at destination.
    if (rUnit.GetOrder().has_value() && &rUnit.GetTile() == rOrder.pDestination)
    {
        rUnit.ClearOrder();
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

} // namespace ac
