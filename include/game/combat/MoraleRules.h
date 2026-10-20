#pragma once

#include "game/combat/CombatRules.h"
#include "game/effects/ActiveEffect.h"

#include <optional>
#include <span>
#include <string>

namespace ac
{

class MoraleCalculator;
class Unit;
struct CombatResult_t;

// Live morale level for combat (SE bonus, morale scales, riot, defense floor).
int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale);

// Effects declared on the unit's effective morale level (empty if level missing).
std::span<const EffectConfig_t> EffectiveLevelEffects(const Unit& rUnit,
                                                      const EffectContext_t& rCtx,
                                                      const MoraleCalculator& rMorale);

// The side that may be promoted: the survivor of a fight in which the other side died.
std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t& rResult);

// The intrinsic rank name, matching stored XP rather than the SE-shifted level.
const std::string& MoraleDisplayName(const Unit& rUnit, const MoraleCalculator& rMorale);

} // namespace ac
