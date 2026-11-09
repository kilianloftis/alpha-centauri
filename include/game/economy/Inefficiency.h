#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/social-engineering/SocialRatingResolver.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

// Fallback HQ distance for energy inefficiency when the faction has no Headquarters.
constexpr int k_DefaultInefficiencyHqDistance = 16;

// The Efficiency table's inefficiency_denominator for a clamped total; ≤ 0 loses all. Throws
// if the level has none.
inline int InefficiencyDenominatorForRating(const SocialRatingRegistry& rRatings,
                                            int efficiencyRating)
{
    const SocialRatingConfig_t& rConfig = rRatings.Get(SocialRatingIdToString(SocialRatingId_t::Efficiency));
    const std::vector<EffectConfig_t>* pLevelEffects =
        FindSocialRatingLevelEffects(rConfig, efficiencyRating);
    if (!pLevelEffects)
    {
        throw std::runtime_error(
            "Efficiency rating " + std::to_string(efficiencyRating)
            + " has no level effects (inefficiency_denominator) configured");
    }

    for (const EffectConfig_t& rEffect : *pLevelEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == StatId_t::InefficiencyDenominator)
        {
            return static_cast<int>(pMod->amount);
        }
    }

    throw std::runtime_error(
        "Efficiency rating level is missing an inefficiency_denominator StatModifier");
}

// The Efficiency table's allocation_imbalance_factor for a clamped total. Throws if the level
// has none.
inline int AllocationImbalanceFactorForRating(const SocialRatingRegistry& rRatings,
                                              int efficiencyRating)
{
    const SocialRatingConfig_t& rConfig =
        rRatings.Get(SocialRatingIdToString(SocialRatingId_t::Efficiency));
    const std::vector<EffectConfig_t>* pLevelEffects =
        FindSocialRatingLevelEffects(rConfig, efficiencyRating);
    if (!pLevelEffects)
    {
        throw std::runtime_error(
            "Efficiency rating " + std::to_string(efficiencyRating)
            + " has no level effects (allocation_imbalance_factor) configured");
    }

    for (const EffectConfig_t& rEffect : *pLevelEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (pMod && pMod->stat == StatId_t::AllocationImbalanceFactor)
        {
            return static_cast<int>(pMod->amount);
        }
    }

    throw std::runtime_error(
        "Efficiency rating level is missing an allocation_imbalance_factor StatModifier");
}

// Energy × Distance / denominator, at most Energy; denominator ≤ 0 loses all.
inline int CalculateInefficiencyLoss(int energy, int distance, int denominator)
{
    if (energy <= 0 || distance <= 0)
    {
        return 0;
    }

    if (denominator <= 0)
    {
        return energy;
    }

    return std::min(energy, (energy * distance) / denominator);
}

} // namespace ac
