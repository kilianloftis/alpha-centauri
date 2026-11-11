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

// The side that may be promoted: the survivor of a fight in which the other side died, unless
// the kill earns none, and the attacker after a boarding.
std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t& rResult);

// False for a captured_on_defeat unit (terranx-map.md "Alien artifacts").
bool KillEarnsPromotion(const Unit& rDefeated);

// Eligibility × success for the survivor. Uses effective morale; rCtx needs combatRole and
// targetTile (battle tile) so WorldGlobal base MinClamp and IsDefending can apply.
double CombatPromotionChance(const Unit& rUnit, CombatSide_t side, int attackStrength,
                             int defenseStrength, const MoraleCalculator& rMorale,
                             const EffectContext_t& rCtx);

// Rank word: stated level with combatRole None (SE and home Crèche; not riot or extras).
const std::string& MoraleDisplayName(const Unit& rUnit, const EffectContext_t& rCtx,
                                     const MoraleCalculator& rMorale);

} // namespace ac
