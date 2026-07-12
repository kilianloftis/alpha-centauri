#include "game/units/UnitOrderExecutor.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"
#include "game/units/MovementRules.h"
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

    if (&rUnit.GetTile() == rOrder.pDestination)
    {
        rUnit.ClearOrder();
        return;
    }

    if (rUnit.GetMovesRemaining() <= 0)
    {
        // Keep the order for the next turn when moves refresh.
        return;
    }

    // Pathfinding is recalculated every step (fog / new info later). The greedy
    // ProposeNextStep seam is temporary until a real pathfinder lands.
    const Tile* pNext = ProposeNextStep(rUnit, *rOrder.pDestination, rWorldMap);
    if (!pNext)
    {
        // No legal improving step — TryStep the desired bump tile so occupant/ZOC
        // blockers are contact-revealed (TryStep is a no-op move on those outcomes).
        const Tile* pDesired = ProposeDesiredStep(rUnit, *rOrder.pDestination, rWorldMap);
        if (pDesired)
        {
            TryStep(rUnit, *pDesired, rWorldMap);
        }
        return;
    }

    if (!TryStep(rUnit, *pNext, rWorldMap))
    {
        return;
    }

    // Combat stub clears the order; otherwise clear when the destination is reached.
    if (rUnit.GetOrder().has_value() && &rUnit.GetTile() == rOrder.pDestination)
    {
        rUnit.ClearOrder();
    }
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldOrder_t& rOrder)
{
    // Hold indefinitely — nothing to do each turn
    (void)rUnit;
    (void)rWorldMap;
    (void)rOrder;
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldUntilHealedOrder_t& rOrder)
{
    // TODO: Clear order when unit reaches full HP
    (void)rUnit;
    (void)rWorldMap;
    (void)rOrder;
}

void UnitOrderExecutor::Execute_(Unit& rUnit, WorldMap& rWorldMap, HoldForTurnsOrder_t& rOrder)
{
    (void)rWorldMap;
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
