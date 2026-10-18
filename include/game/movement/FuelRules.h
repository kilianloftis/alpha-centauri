#pragma once

namespace ac
{

class Unit;
class WorldMap;

// True when rUnit can refuel here: a territory pad that TileHarbors its domain, or embarked
// on a carrier that carries its domain. Co-located but unembarked air over a full deck does
// not count.
bool IsRefuelSite(const Unit& rUnit, const WorldMap& rWorldMap);

// HP a unit loses for ending a turn at 0 fuel: DamageFromOutOfFuel percent of its max HP.
int OutOfFuelDamage(const Unit& rUnit);

struct EndOfTurnFuel_t
{
    // An airborne unit lands on a carrier on its tile before the fuel check.
    bool bBoardsCarrier = false;
    bool bRefuels = false;
    // Away from a refuel site, unused moves burn fuel for the airborne turn.
    bool bBurnsFuel = false;
    // The burn empties the tank, which costs outOfFuelDamage HP.
    bool bOutOfFuel = false;
    int outOfFuelDamage = 0;
    bool bDestroyed = false;
};

// What ending the turn does to a fuel-using unit; nothing for any other.
EndOfTurnFuel_t ResolveEndOfTurnFuel(const Unit& rUnit, const WorldMap& rWorldMap);

// True when ending this turn where it is would destroy the unit (Needlejet / Missile at its
// last turn; not a healthy Copter), and it still has moves and no order to go elsewhere.
bool NeedsAutoReturnToFuel(const Unit& rUnit, const WorldMap& rWorldMap);

} // namespace ac
