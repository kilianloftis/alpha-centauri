#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include <map>
#include <string>
#include <vector>

namespace ac
{

class SocialPolicyRegistry;

class SocialEngineeringManager
{
public:
    explicit SocialEngineeringManager(const SocialPolicyRegistry* pRegistry);
    ~SocialEngineeringManager();

    // Set the active policy for a category. Returns false if the id is not found.
    bool SetActivePolicy(SocialCategory category, const std::string& policyId);

    // Get the active policy config for a category. Returns nullptr if none is set.
    const SocialPolicyConfig* GetActivePolicy(SocialCategory category) const;

    // Compute the aggregate SocialScores from all active policies.
    SocialScores GetCombinedScores() const;

    // All policies in a category that the faction may currently adopt,
    // given the faction's discovered tech string ids.
    std::vector<const SocialPolicyConfig*> GetAvailablePolicies(
        SocialCategory category,
        const std::vector<std::string>& rDiscoveredTechIds) const;

private:
    const SocialPolicyRegistry* m_pRegistry;
    std::map<SocialCategory, std::string> m_activePolicyIds;
};

} // namespace ac
