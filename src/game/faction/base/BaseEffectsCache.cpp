#include "game/faction/base/BaseEffectsCache.h"

#include "game/IEffectsProvider.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/social-engineering/SocialRatingResolver.h"

#include <map>

namespace ac
{

BaseEffectsCache::BaseEffectsCache(const BaseManager& rBase,
                                   const SocialRatingRegistry& rSocialRatings,
                                   const IEffectsProvider& rProvider)
    : m_rBase(rBase)
    , m_rSocialRatings(rSocialRatings)
    , m_pProvider(&rProvider)
    , m_cached(rBase)
{
}

void BaseEffectsCache::BindProvider(const IEffectsProvider& rProvider)
{
    m_pProvider = &rProvider;
    m_cachedPoolVersion.reset();
}

BaseEffects_t BaseEffectsCache::CollectBaseLocal_(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = FilterForBase(rFactionEffects, m_rBase);

    const std::vector<ActiveEffect_t> popEffects =
        CollectFromPops(m_rBase.GetPopulation(), m_rBase);
    baseEffects.effects.insert(baseEffects.effects.end(), popEffects.begin(), popEffects.end());

    return baseEffects;
}

BaseEffects_t BaseEffectsCache::CollectRatingSource() const
{
    return CollectBaseLocal_(m_pProvider->GetLocalActiveEffects());
}

BaseEffects_t BaseEffectsCache::Build(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = CollectBaseLocal_(rFactionEffects);

    // Ratings are a faction-internal axis: accumulate from the local pool only, then append
    // the level effects onto the composed base list (which may also carry world/council
    // StatModifiers). The two lists differ, which is why the resolver returns its effects
    // instead of expanding in place.
    const std::vector<ActiveEffect_t> ratingEffects =
        ResolveSocialRatingLevelEffects(CollectRatingSource(), m_rSocialRatings);
    baseEffects.effects.insert(baseEffects.effects.end(), ratingEffects.begin(),
                               ratingEffects.end());

    return baseEffects;
}

const BaseEffects_t& BaseEffectsCache::Get() const
{
    const uint64_t poolVersion = m_pProvider->GetEffectsVersion();
    if (poolVersion != m_cachedPoolVersion)
    {
        m_cached = Build(m_pProvider->GetActiveEffects());
        m_cachedPoolVersion = poolVersion;
    }
    return m_cached;
}

int BaseEffectsCache::GetEffectiveRating(SocialRatingId_t rating) const
{
    // Local-only ratings: peer WorldGlobal / council SocialRatingModifiers never move this
    // axis (see AccumulateSocialRatings). Same source list as the base-lane expansion.
    const std::map<SocialRatingId_t, int> totals =
        AccumulateSocialRatings(CollectRatingSource().effects);
    const auto it = totals.find(rating);
    return it == totals.end() ? 0 : it->second;
}

} // namespace ac
