#include "game/social-engineering/SocialRatingResolver.h"

#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/effects/BonusEffect.h"

#include <algorithm>
#include <string>

namespace ac
{

std::map<SocialRatingId_t, int> AccumulateSocialRatings(
    const std::vector<ActiveEffect_t>& rEffects)
{
    std::map<SocialRatingId_t, int> totals;
    for (const ActiveEffect_t& rEffect : rEffects)
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

int ClampSocialRatingTotal(const SocialRatingConfig_t& rConfig, int total)
{
    const int minLevel = rConfig.levelEffects.begin()->first;
    const int maxLevel = rConfig.levelEffects.rbegin()->first;
    return std::clamp(total, minLevel, maxLevel);
}

const std::vector<EffectConfig_t>* FindSocialRatingLevelEffects(
    const SocialRatingConfig_t& rConfig, int total)
{
    if (rConfig.levelEffects.empty())
    {
        return nullptr;
    }

    const int level = ClampSocialRatingTotal(rConfig, total);
    const auto it = rConfig.levelEffects.find(level);
    if (it == rConfig.levelEffects.end())
    {
        return nullptr;
    }
    return &it->second;
}

void ExpandSocialRatingEffects(BaseEffects_t& rBaseEffects,
                               const SocialRatingRegistry& rRatings)
{
    const std::map<SocialRatingId_t, int> totals = AccumulateSocialRatings(rBaseEffects.effects);

    for (const auto& [rating, total] : totals)
    {
        if (total == 0)
        {
            continue;
        }

        const SocialRatingConfig_t* pRatingConfig = rRatings.Find(SocialRatingIdToString(rating));
        if (!pRatingConfig || pRatingConfig->levelEffects.empty())
        {
            continue;
        }

        // SMAC: out-of-range totals use the extreme configured level's effects. In-range
        // missing keys (including typical absent 0) still produce nothing.
        const int level = ClampSocialRatingTotal(*pRatingConfig, total);
        const auto it = pRatingConfig->levelEffects.find(level);
        if (it == pRatingConfig->levelEffects.end())
        {
            continue;
        }

        const std::string sourceId = "se_rating_" + SocialRatingIdToString(rating)
                                     + "_" + std::to_string(level);
        for (const auto& rEffect : it->second)
        {
            rBaseEffects.effects.push_back({&rEffect, sourceId, nullptr});
        }
    }
}

void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings)
{
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(rFactionEffects.effects);

    for (const auto& [rating, total] : totals)
    {
        if (total == 0)
        {
            continue;
        }

        const SocialRatingConfig_t* pRatingConfig = rRatings.Find(SocialRatingIdToString(rating));
        if (!pRatingConfig || pRatingConfig->levelEffects.empty())
        {
            continue;
        }

        const int level = ClampSocialRatingTotal(*pRatingConfig, total);
        const auto it = pRatingConfig->levelEffects.find(level);
        if (it == pRatingConfig->levelEffects.end())
        {
            continue;
        }

        const std::string sourceId = "se_rating_" + SocialRatingIdToString(rating)
                                     + "_" + std::to_string(level);
        for (const auto& rEffect : it->second)
        {
            if (LaneFor(rEffect.scope) != EffectLane_t::FactionUnits)
            {
                continue;
            }
            rFactionEffects.effects.push_back({&rEffect, sourceId, nullptr});
        }
    }
}

} // namespace ac
