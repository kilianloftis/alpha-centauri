#pragma once

namespace ac
{

// Roll-pool scale applied to a resolved attack or defense rating.
inline constexpr int k_CombatStrengthScale = 0x100;

class MoraleCalculator;
class TileEffectsContext;
class Unit;

// Roll pools for one fight. attackStrength and defenseStrength are the resolved ratings
// after k_CombatStrengthScale. bPsiCombat is true when either side forces psi combat.
struct CombatStrength_t
{
    int attackStrength = 0;
    int defenseStrength = 0;
    bool bPsiCombat = false;
};

// Attack and defense roll pools. Conventional ratings are the combat Attack/Defense stats;
// psi ratings start at 1 and ignore additive weapon and armour. Defense then multiplies by
// the defender tile's defense multiplier. When the attacker has fewer than one movement
// point remaining, attack strength is multiplied by that leftover fraction of a point.
CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale);

} // namespace ac
