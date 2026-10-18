#pragma once

#include "game/effects/ActiveEffect.h"

#include <span>
#include <string>

namespace ac
{

class MoraleCalculator;
class Unit;

int BaseIntrinsicXp(const Unit& rUnit, const MoraleCalculator& rMorale);

// Live morale level for combat (SE bonus, creche soften, riot, defense floor).
int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale);

// Effects declared on the unit's effective morale level (empty if level missing).
std::span<const EffectConfig_t> EffectiveLevelEffects(const Unit& rUnit,
                                                      const EffectContext_t& rCtx,
                                                      const MoraleCalculator& rMorale);

// The intrinsic rank name, matching stored XP rather than the SE-shifted level.
const std::string& MoraleDisplayName(const Unit& rUnit, const MoraleCalculator& rMorale);

} // namespace ac
