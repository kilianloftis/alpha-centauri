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

// Absolute inefficiency denominator for an Efficiency SE total, read from that axis's
// level table (stat inefficiency_denominator). Clamps like other SE levels. Denom ≤ 0
// means 100% loss. Throws if the (clamped) level has no denominator entry.
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

// Energy lost to inefficiency before the econ/labs/psych split:
//   Inefficiency = Energy × Distance / denominator
// Denominator comes from the Efficiency SE table (see social_rating_effects.json).
// Denominator ≤ 0 loses 100%. Loss never exceeds Energy.
// Distance 0 (faction HQ) yields no loss — callers normally short-circuit HQ bases.
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
