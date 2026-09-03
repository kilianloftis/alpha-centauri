#pragma once

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"

#include <cstdint>
#include <optional>

namespace ac
{

class BaseManager;
class IEffectsProvider;
class SocialRatingRegistry;

// Assembles the effect list a base resolves against, and memoizes it.
//
// Three lists, deliberately distinct:
//   - the *local* list, used only to accumulate this base's social ratings
//   - the *composed* list, which also carries world / council effects
//   - the final list, which is the composed one plus the gameplay effects of the rating
//     levels the local one produced
//
// Ratings are a faction-internal axis, so they must accumulate from the local pool even though
// the base resolves against the composed one. Keeping that asymmetry in one class is the point:
// the number the UI reports and the effects the base resolves cannot disagree.
class BaseEffectsCache
{
public:
    // rBase is the base being assembled for; it must outlive this cache, which it does because
    // BaseManager owns it. The provider is a constructor argument because the cache is unusable
    // without one, and rebindable because a base changes owner.
    BaseEffectsCache(const BaseManager& rBase, const SocialRatingRegistry& rSocialRatings,
                     const IEffectsProvider& rProvider);
    ~BaseEffectsCache() = default;

    // Ownership transfer: the new owner's pool replaces the old one, and the memo is dropped
    // because a different faction's effects are a different answer.
    void BindProvider(const IEffectsProvider& rProvider);

    // The final list. Rebuilt only when the provider's effects version moves, so the reference
    // stays valid until the next effect-source mutation.
    const BaseEffects_t& Get() const;

    // The same assembly against an explicit pool, bypassing the memo. Used while a base is
    // being rebuilt under a new owner, before the pool it will resolve against is live.
    BaseEffects_t Build(const FactionEffects_t& rFactionEffects) const;

    // This base's rating context: the local pool only. The one input to both
    // GetEffectiveSocialRating and the base-lane rating expansion.
    BaseEffects_t CollectRatingSource() const;

    // Accumulated level for one rating axis, from the local pool.
    int GetEffectiveRating(SocialRatingId_t rating) const;

private:
    // FilterForBase over a faction-wide pool, plus this base's own pop-generated ThisBase
    // effects — everything from that pool that applies to this base, before rating expansion.
    BaseEffects_t CollectBaseLocal_(const FactionEffects_t& rFactionEffects) const;

    const BaseManager& m_rBase;
    const SocialRatingRegistry& m_rSocialRatings;
    const IEffectsProvider* m_pProvider;

    mutable BaseEffects_t m_cached;
    // Empty means never built.
    mutable std::optional<uint64_t> m_cachedPoolVersion;
    mutable std::optional<uint64_t> m_cachedMoodRevision;
};

} // namespace ac
