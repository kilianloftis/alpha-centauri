#pragma once

namespace ac
{

class Unit;
class WorldMap;

// True when rUnit can refuel here: tile improvements (Base / Airbase) for anyone on the
// pad, or embarked on a carrier that projects RefuelsAir. Co-located but unembarked air
// over a full deck does not count.
bool IsRefuelSite(const Unit& rUnit);

// End-of-turn fuel for one unit: unembarked air first tries to land on a boardable carrier
// on its tile; then refill on a refuel site, otherwise burn remaining moves; at 0 fuel
// apply DamageFromOutOfFuel (% of max HP) and destroy if HP reaches 0.
void ProcessFuelAtTurnEnd(Unit& rUnit, const WorldMap& rWorldMap);

class GameState;

// Run ProcessFuelAtTurnEnd for every live unit in every faction (deferred destruction).
void ProcessAllFuelAtTurnEnd(GameState& rGameState);

} // namespace ac
