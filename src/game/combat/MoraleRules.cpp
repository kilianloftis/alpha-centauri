#include "game/combat/MoraleRules.h"

#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/effects/EffectEnums.h"
#include "game/units/Unit.h"

#include <algorithm>

namespace ac
{

int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale)
{
    const int liveBonus = ResolveCombatUnitStat(rUnit, StatId_t::MoraleRank, rCtx);
    return rMorale.MoraleLevel(rUnit.GetXp(), liveBonus);
}

int CombatMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                      const MoraleCalculator& rMorale)
{
    const int stated = EffectiveMoraleLevel(rUnit, rCtx, rMorale);
    const int extras = ResolveCombatUnitStat(rUnit, StatId_t::MoraleExtra, rCtx);
    const int combined = stated + extras;
    if (rCtx.combatRole != CombatRole_t::Defender)
    {
        return combined;
    }
    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    return std::clamp(combined, rConfig.defenseFloorIndex, rConfig.MaxLevel());
}

std::optional<CombatSide_t> PromotionCandidate(const CombatResult_t& rResult)
{
    if (rResult.bDefenderDestroyed && !rResult.bAttackerDestroyed)
    {
        return CombatSide_t::Attacker;
    }
    if (rResult.bAttackerDestroyed && !rResult.bDefenderDestroyed)
    {
        return CombatSide_t::Defender;
    }
    return std::nullopt;
}

const std::string& MoraleDisplayName(const Unit& rUnit, const EffectContext_t& rCtx,
                                     const MoraleCalculator& rMorale)
{
    const int level = EffectiveMoraleLevel(rUnit, rCtx, rMorale);
    const MoraleLifecycle_t lifecycle = rUnit.GetDesign().IsNativeLife()
                                            ? MoraleLifecycle_t::Native
                                            : MoraleLifecycle_t::Conventional;
    return rMorale.DisplayName(level, lifecycle);
}

} // namespace ac
