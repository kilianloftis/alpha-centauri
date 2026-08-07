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

// Maps each non-zero accumulated level through the registry table and appends that level's
// gameplay effects to rOut with sourceId "se_rating_<axis>_<level>" (clamped level).
// appendLane, when set, keeps only level effects on that lane — the faction lane emits
// FactionUnits only, while the base lane takes the whole level.
void AppendRatingLevelEffects_(const std::map<SocialRatingId_t, int>& rTotals,
                               const SocialRatingRegistry& rRatings,
                               std::optional<EffectLane_t> appendLane,
                               std::vector<ActiveEffect_t>& rOut)
{
    for (const auto& [rating, total] : rTotals)
    {
        if (total == 0)
        {
            continue;
        }

        // A non-zero total means something declared a modifier on this axis, so a missing table
        // is a config defect rather than "this axis does nothing". ValidateEffectReferences
        // rejects such a modifier at load; this is the backstop, and it names the axis because
        // Registry::Get's message would not say what kind of id it failed to find.
        const std::string axisId = SocialRatingIdToString(rating);
        const SocialRatingConfig_t* pRatingConfig = rRatings.Find(axisId);
        if (!pRatingConfig)
        {
            throw std::runtime_error("Social rating axis '" + axisId + "' accumulated "
                                     + std::to_string(total)
                                     + " but has no table in the social rating registry");
        }
        const SocialRatingConfig_t& rRatingConfig = *pRatingConfig;

        // SMAC: out-of-range totals use the extreme configured level's effects. In-range
        // missing keys (including typical absent 0) still produce nothing.
        const std::vector<EffectConfig_t>* pLevelEffects =
            FindSocialRatingLevelEffects(rRatingConfig, total);
        if (!pLevelEffects)
        {
            continue;
        }

        // Non-null level effects imply a non-empty table, so clamping is safe here.
        const int level = ClampSocialRatingTotal(rRatingConfig, total);
        const std::string sourceId = "se_rating_" + SocialRatingIdToString(rating)
                                     + "_" + std::to_string(level);
        for (const EffectConfig_t& rEffect : *pLevelEffects)
        {
            if (appendLane && LaneFor(rEffect.scope) != *appendLane)
            {
                continue;
            }
            rOut.emplace_back(rEffect, sourceId);
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
    AppendRatingLevelEffects_(AccumulateSocialRatings(rRatingSource.effects), rRatings,
                              std::nullopt, levelEffects);
    return levelEffects;
}

void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings)
{
    // Faction-lane expansion only accumulates FactionWide modifiers. ThisBase rating mods
    // stay on the per-base path after FilterForBase, so N bases with ThisBase shrines
    // cannot inflate FactionUnits bonuses.
    AppendRatingLevelEffects_(
        AccumulateSocialRatings(rFactionEffects.effects, EffectLane_t::FactionWide), rRatings,
        EffectLane_t::FactionUnits, rFactionEffects.effects);
}

} // namespace ac
