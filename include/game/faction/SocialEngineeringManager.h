#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/effects/EffectEnums.h"
#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"
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
    // A reference: the composition root always supplies the registry, and the null branch used
    // to skip the very default-policy validation its own comment called "fail fast".
    // (The rating registry parameter this used to take was stored and never read.)
    explicit SocialEngineeringManager(const SocialPolicyRegistry& rRegistry);
    ~SocialEngineeringManager();

    // Set the active policy for its category (rPolicy.category). Throws if the
    // policy is not in the registry (when a registry is bound).
    void SetActivePolicy(const SocialPolicyConfig_t& rPolicy);

    // Get the active policy config for a category. Returns nullptr if none is set.
    const SocialPolicyConfig_t* GetActivePolicy(SocialCategory_t category) const;

    // Collect all active effects from the current policy selections.
    // SocialRatingModifier effects are accumulated per-rating axis and expanded
    // through the SocialRatingRegistry into their final gameplay EffectConfig_t entries.
    std::vector<ActiveEffect_t> CollectEffects() const;

    // Faction-wide social rating on one axis from active policies only.
    // Base-local modifiers (buildings, pops) live on BaseManager::GetEffectiveSocialRating.
    int GetSocialRating(SocialRatingId_t rating) const;

    // All policies in a category that the faction may currently adopt,
    // given the faction's discovered tech string ids.
    std::vector<const SocialPolicyConfig_t*> GetAvailablePolicies(
        SocialCategory_t category,
        const std::vector<std::string>& rDiscoveredTechIds) const;

    // Bumped on every policy change; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    const SocialPolicyRegistry& m_rRegistry;
    std::map<SocialCategory_t, const SocialPolicyConfig_t*> m_activePolicies;
    Revision m_revision;
};

} // namespace ac
