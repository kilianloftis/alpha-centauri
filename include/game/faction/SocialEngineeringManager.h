#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/Revision.h"
#include "lib/effects/ActiveEffect.h"
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
    SocialEngineeringManager(const SocialPolicyRegistry* pRegistry,
                              const SocialRatingRegistry* pRatingRegistry);
    ~SocialEngineeringManager();

    // Set the active policy for a category. Returns false if the id is not found.
    bool SetActivePolicy(SocialCategory category, const std::string& policyId);

    // Get the active policy config for a category. Returns nullptr if none is set.
    const SocialPolicyConfig* GetActivePolicy(SocialCategory category) const;

    // Collect all active effects from the current policy selections.
    // SocialRatingModifier effects are accumulated per-rating axis and expanded
    // through the SocialRatingRegistry into their final gameplay EffectConfig_t entries.
    std::vector<ActiveEffect_t> CollectEffects() const;

    // All policies in a category that the faction may currently adopt,
    // given the faction's discovered tech string ids.
    std::vector<const SocialPolicyConfig*> GetAvailablePolicies(
        SocialCategory category,
        const std::vector<std::string>& rDiscoveredTechIds) const;

    // Bumped on every policy change; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    const SocialPolicyRegistry* m_pRegistry;
    const SocialRatingRegistry* m_pRatingRegistry;
    std::map<SocialCategory, std::string> m_activePolicyIds;
    Revision m_revision;
};

} // namespace ac
