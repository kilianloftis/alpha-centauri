#pragma once

namespace ac
{

class Pathfinder;
class Unit;
class WorldMap;

// True when rUnit can refuel here: tile improvements (Base / Airbase) for anyone on the
// pad, or embarked on a carrier that projects RefuelsAir. Co-located but unembarked air
// over a full deck does not count.
bool IsRefuelSite(const Unit& rUnit);

// True when ending this turn away from a refuel site would burn the unit to 0 fuel and
// apply lethal DamageFromOutOfFuel (Needlejet / Missile at last turn; not a healthy Copter).
// Requires moves remaining and no existing order.
bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap);

// When NeedsAutoReturnToFuel, path to the cheapest reachable friendly Base / territory
// Airbase / boardable RefuelsAir carrier and assign a MoveOrder. Returns true if an order
// was set. Does not execute the order.
bool TryAssignAutoReturnToFuel(Unit& rUnit, const Pathfinder& rPathfinder);

// End-of-turn fuel for one unit: unembarked air first tries to land on a boardable carrier
// on its tile; then refill on a refuel site, otherwise burn remaining moves; at 0 fuel
// apply DamageFromOutOfFuel (% of max HP) and destroy if HP reaches 0.
void ProcessFuelAtTurnEnd(Unit& rUnit, const WorldMap& rWorldMap);

class GameState;

// Run ProcessFuelAtTurnEnd for every live unit in every faction (deferred destruction).
void ProcessAllFuelAtTurnEnd(GameState& rGameState);

} // namespace ac
