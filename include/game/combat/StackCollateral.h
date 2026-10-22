#pragma once

namespace ac
{

class MoraleCalculator;
class Unit;

// Splash onto the defender's tile-mates. Wild natives die outright and pay pearls; with
// NonCombatantsDestroyedWithoutCombatant, non-combatants die once no combatant is left.
void ApplyStackCollateral(const MoraleCalculator& rMorale, Unit& rAttacker, Unit& rDefender);

} // namespace ac
