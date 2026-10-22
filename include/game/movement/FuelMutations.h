#pragma once

namespace ac
{

class GameState;
class Pathfinder;
class Unit;

// When NeedsAutoReturnToFuel, orders a move to the cheapest reachable pad or carrier. True if
// an order was set.
bool TryAssignAutoReturnToFuel(Unit& rUnit, const Pathfinder& rPathfinder);

// End-of-turn fuel: air lands on a carrier on its tile, refuels at a site, or burns its moves;
// at 0 fuel it takes DamageFromOutOfFuel and may die.
void ProcessFuelAtTurnEnd(Unit& rUnit);

// Run ProcessFuelAtTurnEnd for every live unit in every faction (deferred destruction).
void ProcessAllFuelAtTurnEnd(GameState& rGameState);

} // namespace ac
