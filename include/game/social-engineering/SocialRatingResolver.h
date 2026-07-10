#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/social-engineering/SocialRatingConfig.h"

#include <map>
#include <vector>

namespace ac
{

class SocialRatingRegistry;

// Two-level social rating resolution.
//
// SocialRatingModifier effects are ordinary effects that can be declared by ANY source
// (social policy, building, pop type, ...) at any scope. Because accumulation runs on an
// already-context-filtered effect list, per-base effective ratings fall out naturally:
// a policy's FactionGlobal +2 Growth passes FilterForBase for every base, while a
// building's ThisBase +1 Growth passes only for its own base — that base totals 3,
// every other base totals 2.

// Sums SocialRatingModifier contributions per rating axis over rBaseEffects.
std::map<SocialRatingId_t, int> AccumulateSocialRatings(const BaseEffects_t& rBaseEffects);

// SMAC rule: totals below the lowest / above the highest configured level use that
// extreme's effects. Returns the level key to look up in levelEffects after clamping
// into [min, max] of the table. Caller must ensure levelEffects is non-empty.
int ClampSocialRatingTotal(const SocialRatingConfig_t& rConfig, int total);

// Clamps `total` into the configured range, then exact-matches. Returns nullptr when the
// table is empty or the (possibly clamped) level has no entry — typical for absent 0, or
// sparse in-range gaps in incomplete tables.
const std::vector<EffectConfig_t>* FindSocialRatingLevelEffects(
    const SocialRatingConfig_t& rConfig, int total);

// Appends the gameplay effects each accumulated rating level maps to (via the rating
// registry's levelEffects table, SMAC clamp-at-extremes) to rBaseEffects, with sourceId
// "se_rating_<axis>_<level>" using the clamped level. Both functions take BaseEffects_t
// because accumulation is only meaningful after the list has been filtered to its final
// base context (FilterForBase + pop merge) — that is what attributes ThisBase-scoped
// rating modifiers correctly.
void ExpandSocialRatingEffects(BaseEffects_t& rBaseEffects,
                               const SocialRatingRegistry& rRatings);

} // namespace ac
