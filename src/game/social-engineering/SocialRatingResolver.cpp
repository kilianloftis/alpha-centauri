#include "game/social-engineering/SocialRatingResolver.h"

#include "game/social-engineering/SocialRatingConfig.h"
#include "game/social-engineering/SocialRatingRegistry.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

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

// What units or the faction itself resolve: FactionUnits effects, and FactionGlobal ones other
// than Base-domain stat modifiers. These expand once, at the faction's rating.
bool ExpandsAtFactionLevel_(const EffectConfig_t& rEffect)
{
    if (rEffect.scope == EffectScope_t::FactionUnits)
    {
        return true;
    }
    if (rEffect.scope != EffectScope_t::FactionGlobal)
    {
        return false;
    }
    const auto* pModifier = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    return !pModifier || DomainFor(pModifier->stat) != ResolveDomain_t::Base;
}

// Appends each axis's level effects ("se_rating_<axis>_<level>"), level-0 rows included. Total
// 0 matches only an exact "0" key; other totals clamp to the extremes (SMAC).
void AppendRatingLevelEffects_(const std::map<SocialRatingId_t, int>& rTotals,
                               const SocialRatingRegistry& rRatings,
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
            rOut.emplace_back(rEffect, sourceId, nullptr);
        }
    }
}

} // namespace

std::map<SocialRatingId_t, int> AccumulateSocialRatings(
    const std::vector<ActiveEffect_t>& rEffects,
    std::optional<EffectLane_t> laneFilter)
{
    std::set<SocialPolicyId_t> impunityPolicies;
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (laneFilter && LaneFor(rEffect.config->scope) != *laneFilter)
        {
            continue;
        }
        if (const auto* pImpunity =
                std::get_if<SocialPolicyImpunityEffect_t>(&rEffect.config->effect))
        {
            impunityPolicies.insert(pImpunity->policy);
        }
    }

    std::map<SocialRatingId_t, int> totals;
    std::map<SocialRatingId_t, int> minClamps;
    std::map<SocialRatingId_t, int> maxClamps;
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        if (laneFilter && LaneFor(rEffect.config->scope) != *laneFilter)
        {
            continue;
        }
        const auto* pRatingMod =
            std::get_if<SocialRatingModifierEffect_t>(&rEffect.config->effect);
        if (!pRatingMod)
        {
            continue;
        }
        if (pRatingMod->op == ModifierOp_t::Add)
        {
            if (pRatingMod->amount < 0
                && impunityPolicies.count(rEffect.sourceId) != 0)
            {
                continue;
            }
            totals[pRatingMod->rating] += pRatingMod->amount;
            continue;
        }
        if (pRatingMod->op == ModifierOp_t::MinClamp)
        {
            const auto it = minClamps.find(pRatingMod->rating);
            if (it == minClamps.end())
            {
                minClamps.emplace(pRatingMod->rating, pRatingMod->amount);
            }
            else
            {
                it->second = std::max(it->second, pRatingMod->amount);
            }
            continue;
        }
        // MaxClamp — parser rejects any other op.
        const auto it = maxClamps.find(pRatingMod->rating);
        if (it == maxClamps.end())
        {
            maxClamps.emplace(pRatingMod->rating, pRatingMod->amount);
        }
        else
        {
            it->second = std::min(it->second, pRatingMod->amount);
        }
    }

    // MaxClamp then MinClamp, so MinClamp wins a crossing pair (ApplyModifierStack).
    for (const auto& [rating, maxClamp] : maxClamps)
    {
        int& rTotal = totals[rating];
        rTotal = std::min(rTotal, maxClamp);
    }
    for (const auto& [rating, minClamp] : minClamps)
    {
        int& rTotal = totals[rating];
        rTotal = std::max(rTotal, minClamp);
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
    AppendRatingLevelEffects_(totals, rRatings, levelEffects);
    // These reach the base through the faction pool, and units through it alone; a base list
    // holding them too would count a level twice.
    std::erase_if(levelEffects, [](const ActiveEffect_t& rEffect)
                  { return ExpandsAtFactionLevel_(*rEffect.config); });
    return levelEffects;
}

void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings)
{
    // FactionWide modifiers only, so ThisBase shrines cannot inflate faction-level effects.
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(rFactionEffects.effects, EffectLane_t::FactionWide);
    std::vector<ActiveEffect_t> levelEffects;
    AppendRatingLevelEffects_(totals, rRatings, levelEffects);
    for (const ActiveEffect_t& rEffect : levelEffects)
    {
        if (ExpandsAtFactionLevel_(*rEffect.config))
        {
            rFactionEffects.effects.push_back(rEffect);
        }
    }
}

} // namespace ac
