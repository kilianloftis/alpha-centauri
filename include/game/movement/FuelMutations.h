#pragma once

namespace ac
{

class GameState;
class Pathfinder;
class Unit;

// When NeedsAutoReturnToFuel, path to the cheapest reachable friendly harbor pad /
// boardable carrier and assign a MoveOrder. Returns true if an order was set. Does not
// execute the order.
bool TryAssignAutoReturnToFuel(Unit& rUnit, const Pathfinder& rPathfinder);

// End-of-turn fuel for one unit: unembarked air first tries to land on a boardable carrier
// on its tile; then refill on a refuel site, otherwise burn remaining moves; at 0 fuel
// apply DamageFromOutOfFuel (% of max HP) and destroy if HP reaches 0.
void ProcessFuelAtTurnEnd(Unit& rUnit);

// Run ProcessFuelAtTurnEnd for every live unit in every faction (deferred destruction).
void ProcessAllFuelAtTurnEnd(GameState& rGameState);

} // namespace ac
