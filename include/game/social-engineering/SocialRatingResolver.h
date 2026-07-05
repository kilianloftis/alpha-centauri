#pragma once

#include "lib/effects/ActiveEffect.h"
#include "lib/effects/EffectEnums.h"

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

// Sums SocialRatingModifier contributions per rating axis over rEffects.
std::map<SocialRatingId, int> AccumulateSocialRatings(const std::vector<ActiveEffect_t>& rEffects);

// Appends the gameplay effects each accumulated rating level maps to (via the rating
// registry's levelEffects table, exact-level match as before) to rEffects, with sourceId
// "se_rating_<axis>_<total>". Call after the list has been filtered to its final context
// (e.g. after FilterForBase) so ThisBase-scoped rating modifiers are attributed correctly.
void ExpandSocialRatingEffects(std::vector<ActiveEffect_t>& rEffects,
                               const SocialRatingRegistry& rRatings);

} // namespace ac
