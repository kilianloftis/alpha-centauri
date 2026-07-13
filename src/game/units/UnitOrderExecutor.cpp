#include "game/units/UnitOrderExecutor.h"

#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/MoveCostCalculator.h"
#include "game/units/MovementRules.h"
#include "game/faction/FactionRevealedUnits.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/Faction.h"

#include <stdexcept>

namespace ac
{

UnitOrderExecutor::UnitOrderExecutor(const ImprovementRegistry& rImprovements,
                                     WorldMap& rWorldMap,
                                     const TileEffectsContext& rTileEffects)
    : m_steps(rImprovements, rWorldMap, rTileEffects)
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

void UnitOrderExecutor::ResolveCombatStub_(Unit& rAttacker)
{
    rAttacker.SetMoveFragmentsRemaining(0);
    rAttacker.ClearOrder();
}

bool UnitOrderExecutor::TryEnterTile_(Unit& rMover, const Tile& rTo, int remainingAfter)
{
    m_steps.GetWorldMap().GetUnitPositions().MoveUnit(rMover, rTo);
    rMover.SetMoveFragmentsRemaining(remainingAfter);
    return true;
}

void UnitOrderExecutor::ClearFungusCharge_(MoveOrder_t& rMoveOrder)
{
    rMoveOrder.pFungusChargeTile = nullptr;
    rMoveOrder.fungusFragmentsPaid = 0;
}

bool UnitOrderExecutor::TryFungusStep_(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder)
{
    WorldMap& rWorldMap = m_steps.GetWorldMap();

    if (HasFriendlyOccupant(rMover, rTo, rWorldMap))
    {
        ClearFungusCharge_(rMoveOrder);
        return TryEnterTile_(rMover, rTo, /*remainingAfter=*/0);
    }

    const UnitMoveProfile_t profile = MoveProfileFor(rMover);
    const int cost = m_steps.GetMoveCosts().ComputeFragments(rTo, profile);
    const int available = rMover.GetMoveFragmentsRemaining();

    if (rMoveOrder.pFungusChargeTile != &rTo)
    {
        rMoveOrder.pFungusChargeTile = &rTo;
        rMoveOrder.fungusFragmentsPaid = 0;
    }
    rMoveOrder.fungusFragmentsPaid += available;

    if (rMoveOrder.fungusFragmentsPaid >= cost)
    {
        ClearFungusCharge_(rMoveOrder);
        return TryEnterTile_(rMover, rTo, /*remainingAfter=*/0);
    }

    // Charged this turn's moves toward entry; stay put.
    rMover.SetMoveFragmentsRemaining(0);
    return false;
}

bool UnitOrderExecutor::TryStep(Unit& rMover, const Tile& rTo, MoveOrder_t& rMoveOrder)
{
    const StepEvaluation_t eval = m_steps.EvaluateStep(rMover, rTo);
    if (eval.outcome == StepOutcome_t::Legal)
    {
        if (UsesFungusEntryRules(rMover, rTo))
        {
            return TryFungusStep_(rMover, rTo, rMoveOrder);
        }

        const int cost = m_steps.GetMoveCosts().ComputeFragments(rTo, MoveProfileFor(rMover));
        ClearFungusCharge_(rMoveOrder);
        return TryEnterTile_(rMover, rTo, rMover.GetMoveFragmentsRemaining() - cost);
    }

    if (eval.outcome == StepOutcome_t::BlockedByOccupant
        || eval.outcome == StepOutcome_t::BlockedByZoc)
    {
        RevealBlockingUnits_(rMover, eval);
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
    if (!m_steps.HasVisibleHostileUnit(rAttacker, rTargetTile))
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

    // Pathfinding is recalculated every step (fog / new info later). The greedy
    // ProposeNextStep seam is temporary until a real pathfinder lands.
    const Tile* pNext = m_steps.ProposeNextStep(rUnit, *rOrder.pDestination);
    if (!pNext)
    {
        // No legal improving step — TryStep the desired bump tile so occupant/ZOC
        // blockers are contact-revealed (TryStep is a no-op move on those outcomes).
        const Tile* pDesired = m_steps.ProposeDesiredStep(rUnit, *rOrder.pDestination);
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

    // Combat stub clears the order; otherwise clear when the destination is reached.
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
