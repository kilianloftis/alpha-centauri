#pragma once

#include "game/combat/CombatRules.h"

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

// Attack and defence pools; psi ratings start at 1. The defender uses Defense (Attack in a duel)
// times its tile's multiplier; under one movement point, attack scales by the fraction left.
CombatStrength_t ResolveCombatStrength(const Unit& rAttacker, const Unit& rDefender,
                                       const TileEffectsContext& rTileEffects,
                                       const MoraleCalculator& rMorale,
                                       CombatEngagement_t engagement);

} // namespace ac
