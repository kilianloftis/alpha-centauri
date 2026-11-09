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

// Assembles and memoizes a base's effects. Ratings accumulate from the local list, but the base
// resolves the composed list plus their level effects, so UI and rules agree.
class BaseEffectsCache
{
public:
    // rBase owns this cache. The provider is rebindable, since a base changes owner.
    BaseEffectsCache(const BaseManager& rBase, const SocialRatingRegistry& rSocialRatingRegistry,
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
    const SocialRatingRegistry& m_rSocialRatingRegistry;
    const IEffectsProvider* m_pProvider;

    mutable BaseEffects_t m_cached;
    // Empty means never built.
    mutable std::optional<uint64_t> m_cachedPoolVersion;
    mutable std::optional<uint64_t> m_cachedMoodRevision;
};

} // namespace ac
