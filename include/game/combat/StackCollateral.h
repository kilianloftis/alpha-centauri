#pragma once

namespace ac
{

class MoraleCalculator;
class Unit;
class WorldMap;

// Splash onto other occupants of the defender's tile. Does not destroy the defender.
// A MaxClamp that leaves collateral_susceptibility at 0 skips that occupant. A wild
// native is destroyed outright, and GrantPlanetPearls pays the killer. While
// NonCombatantsDestroyedWithoutCombatant is in force, remaining non-combatants are
// destroyed when no combatant is left on the tile.
void ApplyStackCollateral(WorldMap& rWorldMap, const MoraleCalculator& rMorale,
                          Unit& rAttacker, Unit& rDefender);

} // namespace ac
