#include "game/faction/base/BaseEffectsCache.h"

#include "game/IEffectsProvider.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BaseMoodEffects.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/social-engineering/SocialRatingResolver.h"

#include <map>
#include <span>

namespace ac
{

namespace
{

const EffectConfig_t& PlayerDisableProductionEffect_()
{
    static const EffectConfig_t kEffect = [] {
        EffectConfig_t config;
        config.effect = RuleFlagEffect_t{RuleFlagId_t::DisableProduction};
        config.scope = EffectScope_t::ThisBase;
        config.persistence = EffectPersistence_t::Continuous;
        return config;
    }();
    return kEffect;
}

} // namespace

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
    m_cachedMoodRevision.reset();
    m_cachedPlayerDisabledProduction.reset();
}

BaseEffects_t BaseEffectsCache::CollectBaseLocal_(const FactionEffects_t& rFactionEffects) const
{
    BaseEffects_t baseEffects = FilterForBase(rFactionEffects, m_rBase);

    const std::vector<ActiveEffect_t> popEffects =
        CollectFromPops(m_rBase.GetPopulation(), m_rBase);
    baseEffects.effects.insert(baseEffects.effects.end(), popEffects.begin(), popEffects.end());

    // Base-lane half only: the faction-lane half of the same arrays enters the pool via
    // FactionEffectsPool::CollectMoodEffects_, so nothing is counted on both paths.
    AppendBaseMoodBaseLaneEffects(m_rBase, baseEffects.effects);

    if (m_rBase.HasPlayerDisabledProduction())
    {
        // Same RuleFlag riot tiers emit; player decline of WouldEmptyBase is another source.
        const EffectConfig_t& rEffect = PlayerDisableProductionEffect_();
        AppendBaseLaneEffects(std::span<const EffectConfig_t>(&rEffect, 1), &m_rBase,
                              "production_disabled", baseEffects.effects);
    }

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
    // The mood revision and player-disable bit are separate keys rather than folded into the
    // pool version because a base's own mood / WouldEmptyBase decline also change its
    // base-lane list, which the pool never sees.
    const uint64_t poolVersion = m_pProvider->GetEffectsVersion();
    const uint64_t moodRevision = m_rBase.GetPopulation().GetMoodRevision();
    const bool bPlayerDisabled = m_rBase.HasPlayerDisabledProduction();
    if (poolVersion != m_cachedPoolVersion || moodRevision != m_cachedMoodRevision
        || bPlayerDisabled != m_cachedPlayerDisabledProduction)
    {
        m_cached = Build(m_pProvider->GetActiveEffects());
        m_cachedPoolVersion = poolVersion;
        m_cachedMoodRevision = moodRevision;
        m_cachedPlayerDisabledProduction = bPlayerDisabled;
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
