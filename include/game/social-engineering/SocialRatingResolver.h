#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/social-engineering/SocialRatingConfig.h"

#include <map>
#include <optional>
#include <vector>

namespace ac
{

class SocialRatingRegistry;

// Ratings accumulate over the local pool only, never peer or council effects; per-base totals
// fall out of FilterForBase.
// TODO: reject a WorldGlobal SocialRatingModifier at load; it currently moves nothing.

// Sums rating Add modifiers per axis (skipping negatives whose sourceId has SocialPolicyImpunity
// in the same list), then applies MinClamp / MaxClamp. The faction lane passes FactionWide so
// ThisBase modifiers cannot inflate it.
std::map<SocialRatingId_t, int> AccumulateSocialRatings(
    const std::vector<ActiveEffect_t>& rEffects,
    std::optional<EffectLane_t> laneFilter);

// The level to look up after clamping total to the table (SMAC). Throws on an empty table.
int ClampSocialRatingTotal(const SocialRatingConfig_t& rConfig, int total);

// nullptr when the clamped level has no entry.
const std::vector<EffectConfig_t>* FindSocialRatingLevelEffects(
    const SocialRatingConfig_t& rConfig, int total);

// Base lane: accumulates rRatingSource and returns each axis's level effects other than those
// ExpandFactionLaneSocialRatingEffects carries. Ratings accumulate over the local pool, but
// the base resolves over the composed one.
std::vector<ActiveEffect_t> ResolveSocialRatingLevelEffects(
    const BaseEffects_t& rRatingSource, const SocialRatingRegistry& rRatings);

// Appends, at the faction's rating (FactionWide modifiers), the level effects units or the
// faction resolve: FactionUnits ones, and FactionGlobal ones other than Base-domain stat
// modifiers. So SE Morale reaches units and SE Probe +3's immunity flag reaches the faction.
// ThisBase, AllOwnerBases and FactionGlobal Base-domain effects stay on the base path.
void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings);

} // namespace ac
