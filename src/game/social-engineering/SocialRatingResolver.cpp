#include "game/social-engineering/SocialRatingResolver.h"

#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "lib/effects/BonusEffect.h"

#include <string>

namespace ac
{

std::map<SocialRatingId, int> AccumulateSocialRatings(const BaseEffects_t& rBaseEffects)
{
    std::map<SocialRatingId, int> totals;
    for (const ActiveEffect_t& rEffect : rBaseEffects.effects)
    {
        if (!rEffect.config)
        {
            continue;
        }
        if (const auto* pRatingMod = std::get_if<SocialRatingModifierEffect_t>(&rEffect.config->effect))
        {
            totals[pRatingMod->rating] += pRatingMod->amount;
        }
    }
    return totals;
}

void ExpandSocialRatingEffects(BaseEffects_t& rBaseEffects,
                               const SocialRatingRegistry& rRatings)
{
    const std::map<SocialRatingId, int> totals = AccumulateSocialRatings(rBaseEffects);

    for (const auto& [rating, total] : totals)
    {
        if (total == 0)
        {
            continue;
        }

        const SocialRatingConfig* pRatingConfig = rRatings.Find(SocialRatingIdToString(rating));
        if (!pRatingConfig)
        {
            continue;
        }

        // Exact-level match, same semantics as the previous SocialEngineeringManager
        // implementation: levels not listed in the config produce no effects.
        const auto it = pRatingConfig->levelEffects.find(total);
        if (it == pRatingConfig->levelEffects.end())
        {
            continue;
        }

        const std::string sourceId = "se_rating_" + SocialRatingIdToString(rating)
                                     + "_" + std::to_string(total);
        for (const auto& rEffect : it->second)
        {
            rBaseEffects.effects.push_back({&rEffect, sourceId, nullptr});
        }
    }
}

} // namespace ac
