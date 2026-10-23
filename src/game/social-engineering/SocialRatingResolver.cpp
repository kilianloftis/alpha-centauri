#include "game/social-engineering/SocialRatingResolver.h"

#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/effects/EffectConfig.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

struct LevelRow_t
{
    int level;
    const std::vector<EffectConfig_t>* pEffects;
};

// Total 0 matches only an exact "0" row (absent means none); any other total clamps to the
// table's extremes, and a missing row there is a config defect.
std::optional<LevelRow_t> RatingLevelRow_(const SocialRatingConfig_t& rRatingConfig, int total)
{
    if (total == 0)
    {
        // Exact level 0 only — do not ClampSocialRatingTotal(0) into a table whose
        // lowest key is e.g. Growth 2.
        const auto zeroIt = rRatingConfig.levelEffects.find(0);
        if (zeroIt == rRatingConfig.levelEffects.end())
        {
            return std::nullopt;
        }
        return LevelRow_t{0, &zeroIt->second};
    }
    const std::vector<EffectConfig_t>* pLevelEffects =
        FindSocialRatingLevelEffects(rRatingConfig, total);
    if (!pLevelEffects)
    {
        // Non-zero total with no row after clamp: config defect (sparse gap).
        throw std::runtime_error("Social rating axis '" + rRatingConfig.id + "' accumulated "
                                 + std::to_string(total)
                                 + " but has no level effects for the clamped level");
    }
    return LevelRow_t{ClampSocialRatingTotal(rRatingConfig, total), pLevelEffects};
}

// Appends each axis's level effects ("se_rating_<axis>_<level>"), level-0 rows included. Total
// 0 matches only an exact "0" key; other totals clamp to the extremes (SMAC).
void AppendRatingLevelEffects_(const std::map<SocialRatingId_t, int>& rTotals,
                               const SocialRatingRegistry& rRatings,
                               std::optional<EffectLane_t> appendLane,
                               std::vector<ActiveEffect_t>& rOut)
{
    for (const SocialRatingConfig_t& rRatingConfig : rRatings.GetAll())
    {
        const SocialRatingId_t rating = ParseSocialRatingId(rRatingConfig.id);
        const auto totalIt = rTotals.find(rating);
        const int total = totalIt == rTotals.end() ? 0 : totalIt->second;

        const std::optional<LevelRow_t> row = RatingLevelRow_(rRatingConfig, total);
        if (!row)
        {
            continue;
        }
        const std::string sourceId =
            "se_rating_" + rRatingConfig.id + "_" + std::to_string(row->level);
        for (const EffectConfig_t& rEffect : *row->pEffects)
        {
            if (appendLane && LaneFor(rEffect.scope) != *appendLane)
            {
                continue;
            }
            rOut.emplace_back(rEffect, sourceId, nullptr);
        }
    }
}

} // namespace

std::map<SocialRatingId_t, int> AccumulateSocialRatings(
    const std::vector<ActiveEffect_t>& rEffects,
    std::optional<EffectLane_t> laneFilter)
{
    std::map<SocialRatingId_t, int> totals;
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (laneFilter && LaneFor(rEffect.config->scope) != *laneFilter)
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
    if (rConfig.levelEffects.empty())
    {
        throw std::runtime_error("Social rating '" + rConfig.id + "' has no levels to clamp to");
    }
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

std::vector<ActiveEffect_t> ResolveSocialRatingLevelEffects(
    const BaseEffects_t& rRatingSource, const SocialRatingRegistry& rRatings)
{
    std::vector<ActiveEffect_t> levelEffects;
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(rRatingSource.effects, std::nullopt);
    AppendRatingLevelEffects_(totals, rRatings, std::nullopt, levelEffects);
    return levelEffects;
}

void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings)
{
    // FactionWide modifiers only, so ThisBase shrines cannot inflate FactionUnits bonuses.
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(rFactionEffects.effects, EffectLane_t::FactionWide);
    AppendRatingLevelEffects_(totals, rRatings, EffectLane_t::FactionUnits,
                              rFactionEffects.effects);
}

} // namespace ac
