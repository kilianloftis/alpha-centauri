#include "game/units/UnitOrderExecutor.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/map/WorldMap.h"
#include <stdexcept>

namespace ac
{

void UnitOrderExecutor::Execute(Unit& rUnit, WorldMap& rWorldMap)
{
    if (!rUnit.GetOrder().has_value())
        return;

    std::visit([&](auto& rOrder)
    {
        Execute_(rUnit, rWorldMap, rOrder);
    }, *rUnit.GetOrder()); // non-const overload — allows mutating HoldForTurnsOrder_t
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, MoveOrder_t& rOrder)
{
    if (!rOrder.pDestination)
        throw std::runtime_error("MoveOrder has null destination");

    // TODO: Implement step-by-step pathfinding toward destination
    if (!rWorldMap.GetUnitPositions().TryMoveUnit(rUnit, *rOrder.pDestination))
    {
        // Destination blocked under the single-unit-per-tile rule; keep the order so the
        // move is retried next turn. TODO: blocked-move rules (reroute, cancel, notify
        // the player) are undecided.
        return;
    }
    rUnit.ClearOrder();
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldOrder_t& rOrder)
{
    // Hold indefinitely — nothing to do each turn
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldUntilHealedOrder_t& rOrder)
{
    // TODO: Clear order when unit reaches full HP
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldForTurnsOrder_t& rOrder)
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
