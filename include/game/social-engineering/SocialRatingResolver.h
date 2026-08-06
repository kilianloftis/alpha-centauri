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

// Two-level social rating resolution.
//
// SocialRatingModifier effects are ordinary effects that can be declared by faction-
// internal sources (social policy, building, pop type, unit component, ...). Ratings are
// a *local* axis: accumulation on both lanes uses the faction's local effect pool, not
// peer WorldGlobal or council extras. Until EffectConfigParser rejects a WorldGlobal
// SocialRatingModifier at load (package 5), a mod can declare one and it moves neither
// lane — the restriction is silent, not enforced.
//
// Per-base effective ratings fall out naturally after FilterForBase over the local pool:
// a policy's FactionGlobal +2 Growth passes for every base, while a building's ThisBase
// +1 Growth passes only for its own base — that base totals 3, every other base totals 2.

// Sums SocialRatingModifier contributions per rating axis. Takes a plain effect list —
// callers decide the context (a base's filtered local list, or the faction pool). Do not
// pass composed world/council extras here. laneFilter restricts the contributing
// modifiers to one lane: the faction lane passes FactionWide so that N bases' ThisBase
// modifiers cannot inflate a faction-wide total; base-context callers pass nothing,
// because FilterForBase has already established the context.
std::map<SocialRatingId_t, int> AccumulateSocialRatings(
    const std::vector<ActiveEffect_t>& rEffects,
    std::optional<EffectLane_t> laneFilter = std::nullopt);

// SMAC rule: totals below the lowest / above the highest configured level use that
// extreme's effects. Returns the level key to look up in levelEffects after clamping
// into [min, max] of the table. Caller must ensure levelEffects is non-empty.
int ClampSocialRatingTotal(const SocialRatingConfig_t& rConfig, int total);

// Clamps `total` into the configured range, then exact-matches. Returns nullptr when the
// table is empty or the (possibly clamped) level has no entry — typical for absent 0, or
// sparse in-range gaps in incomplete tables.
const std::vector<EffectConfig_t>* FindSocialRatingLevelEffects(
    const SocialRatingConfig_t& rConfig, int total);

// Base lane: accumulates rRatingSource and returns the gameplay effects each non-zero
// level maps to (via the rating registry's levelEffects table, SMAC clamp-at-extremes),
// with sourceId "se_rating_<axis>_<level>" using the clamped level.
//
// Takes BaseEffects_t because accumulation is only meaningful after the list has been
// filtered to its final base context (FilterForBase + pop merge) — that is what
// attributes ThisBase-scoped rating modifiers to the right base. It *returns* the effects
// rather than appending in place because the list ratings are accumulated from is not
// always the list the base resolves against: ratings accumulate over the local pool,
// while resolution runs over the composed pool (see BaseManager::BuildBaseEffects_).
std::vector<ActiveEffect_t> ResolveSocialRatingLevelEffects(
    const BaseEffects_t& rRatingSource, const SocialRatingRegistry& rRatings);

// Expands FactionWide-scoped SocialRatingModifiers from the local faction pool into
// FactionUnits gameplay effects and appends them (so SE Morale reaches live units).
// ThisBase / Base-lane modifiers are ignored here — they belong only to the per-base
// ResolveSocialRatingLevelEffects path after FilterForBase. FactionWide gameplay effects
// from the level table are also not re-expanded here (base path only).
void ExpandFactionLaneSocialRatingEffects(FactionEffects_t& rFactionEffects,
                                          const SocialRatingRegistry& rRatings);

} // namespace ac
