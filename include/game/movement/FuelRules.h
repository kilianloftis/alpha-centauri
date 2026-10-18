#pragma once

namespace ac
{

class Unit;
class WorldMap;

// True when rUnit can refuel here: a territory pad that TileHarbors its domain, or embarked
// on a carrier that carries its domain. Co-located but unembarked air over a full deck does
// not count.
bool IsRefuelSite(const Unit& rUnit, const WorldMap& rWorldMap);

// True when ending this turn away from a refuel site would burn the unit to 0 fuel and
// apply lethal DamageFromOutOfFuel (Needlejet / Missile at last turn; not a healthy Copter).
// Requires moves remaining and no existing order.
bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap);

// HP a unit loses for ending a turn at 0 fuel: DamageFromOutOfFuel percent of its max HP.
int OutOfFuelDamage(const Unit& rUnit);

} // namespace ac
