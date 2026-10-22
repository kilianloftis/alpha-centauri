#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/effects/EffectEnums.h"
#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ac
{

class SocialPolicyRegistry;
class SocialRatingRegistry;

class SocialEngineeringManager
{
public:
    // rRegistry validates the default policies at once.
    explicit SocialEngineeringManager(const SocialPolicyRegistry& rRegistry);
    ~SocialEngineeringManager();

    // Set the active policy for its category (rPolicy.category). Throws if the
    // policy is not in the registry (when a registry is bound).
    void SetActivePolicy(const SocialPolicyConfig_t& rPolicy);

    // Get the active policy config for a category. Returns nullptr if none is set.
    const SocialPolicyConfig_t* GetActivePolicy(SocialCategory_t category) const;

    // Effects of the current policy selections.
    std::vector<ActiveEffect_t> CollectEffects() const;

    // Faction-wide social rating on one axis from active policies only.
    // Base-local modifiers (buildings, pops) live on BaseManager::GetEffectiveSocialRating.
    int GetSocialRating(SocialRatingId_t rating) const;


    // Bumped on every policy change; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    // Every axis's total, rebuilt only when the selection changes; the UI asks per frame.
    const std::map<SocialRatingId_t, int>& AccumulatedRatings_() const;

    const SocialPolicyRegistry& m_rRegistry;
    std::map<SocialCategory_t, const SocialPolicyConfig_t*> m_activePolicies;
    Revision m_revision;
    mutable std::map<SocialRatingId_t, int> m_cachedRatings;
    // Starts at max so the first query cannot match revision 0 and read an empty cache.
    mutable uint64_t m_cachedRatingsRevision = UINT64_MAX;
};

} // namespace ac
