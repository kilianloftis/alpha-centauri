#pragma once

#include "game/combat/CombatRules.h"
#include "game/effects/ActiveEffect.h"

#include <random>

namespace ac
{

class MoraleCalculator;
class Unit;

// Promote the survivor one intrinsic XP on success. Caps at the top level.
bool TryPromote(Unit& rSurvivor, CombatSide_t side, int attackStrength, int defenseStrength,
                const MoraleCalculator& rMorale, const EffectContext_t& rCtx, std::mt19937& rRng);

} // namespace ac
