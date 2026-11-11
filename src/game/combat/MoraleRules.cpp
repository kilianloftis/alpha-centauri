#include "game/combat/MoraleRules.h"

#include "game/combat/CombatResolver.h"
#include "game/combat/MoraleCalculator.h"
#include "game/effects/EffectEnums.h"
#include "game/natives/NativeRules.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string>

namespace ac
{

int EffectiveMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                         const MoraleCalculator& rMorale)
{
    if (IsWildNative(rUnit))
    {
        return WildNativeLifecycleLevel(rUnit, rMorale);
    }
    const int liveBonus = ResolveCombatUnitStat(rUnit, StatId_t::MoraleRank, rCtx, {});
    return rMorale.MoraleLevel(rUnit.GetXp(), liveBonus);
}

int CombatMoraleLevel(const Unit& rUnit, const EffectContext_t& rCtx,
                      const MoraleCalculator& rMorale)
{
    const int stated = EffectiveMoraleLevel(rUnit, rCtx, rMorale);
    const int extras = ResolveCombatUnitStat(rUnit, StatId_t::MoraleExtra, rCtx, {});
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
    if (rResult.bDefenderBoarded)
    {
        return CombatSide_t::Attacker;
    }
    if (rResult.bDefenderDestroyed && !rResult.bAttackerDestroyed)
    {
        if (!rResult.bKillEarnsPromotion)
        {
            return std::nullopt;
        }
        return CombatSide_t::Attacker;
    }
    if (rResult.bAttackerDestroyed && !rResult.bDefenderDestroyed)
    {
        return CombatSide_t::Defender;
    }
    return std::nullopt;
}

bool KillEarnsPromotion(const Unit& rDefeated)
{
    return !ResolveFlag(rDefeated, RuleFlagId_t::CapturedOnDefeat);
}

double CombatPromotionChance(const Unit& rUnit, CombatSide_t side, int attackStrength,
                             int defenseStrength, const MoraleCalculator& rMorale,
                             const EffectContext_t& rCtx)
{
    const MoraleConfig_t& rConfig = rMorale.GetConfig();
    if (rUnit.GetXp() >= rConfig.MaxLevel())
    {
        return 0.0;
    }
    const int effective = EffectiveMoraleLevel(rUnit, rCtx, rMorale);
    if (effective >= rConfig.MaxLevel())
    {
        return 0.0;
    }
    const MoraleLevel_t* pLevel = rConfig.FindLevel(effective);
    if (!pLevel)
    {
        throw std::runtime_error("No morale level found for effective morale "
                                 + std::to_string(effective));
    }
    const int ourStrength =
        side == CombatSide_t::Attacker ? attackStrength : defenseStrength;
    const int theirStrength =
        side == CombatSide_t::Attacker ? defenseStrength : attackStrength;
    const double eligibilitySeed =
        rMorale.PromotionEligibilitySeed(ourStrength, theirStrength);
    const std::span<const EffectConfig_t> levelEffects = pLevel->effects;
    const double eligibility = ResolveCombatUnitMultiplicativeStat(
        rUnit, StatId_t::PromotionEligibilityChance, eligibilitySeed, rCtx, levelEffects);
    // Success seed is identity; Hardened+ ×0.5 and live modifiers come from effects.
    const double success = ResolveCombatUnitMultiplicativeStat(
        rUnit, StatId_t::PromotionSuccessChance, 1.0, rCtx, levelEffects);
    return eligibility * success;
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
