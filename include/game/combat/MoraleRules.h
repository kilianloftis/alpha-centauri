#pragma once

#include "game/combat/CombatRules.h"
#include "game/effects/ActiveEffect.h"

#include <optional>
#include <string>

namespace ac
{

class MoraleCalculator;
class Unit;
struct CombatResult_t;

// Stated morale level (XP + morale_rank), clamped to the configured ranks.
int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale);

// Combat morale level: stated plus morale_extra. Defense is then clamped to
// [defense_floor_index, MaxLevel]; attack is not.
int CombatMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                      const MoraleCalculator& rMorale);

// The side that may be promoted: the survivor of a fight in which the other side died.
std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t& rResult);

// Rank word: stated level with combatRole None (SE and home Crèche; not riot or extras).
const std::string& MoraleDisplayName(const Unit& rUnit, const EffectContext_t& rCtx,
                                     const MoraleCalculator& rMorale);

} // namespace ac
