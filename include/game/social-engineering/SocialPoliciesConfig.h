#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/UpheavalCostConfig.h"

#include <vector>

namespace ac
{

// Root of config/social_policies.json: upheaval coefficients plus the policy list.
struct SocialPoliciesConfig_t
{
    UpheavalCostConfig_t upheaval;
    std::vector<SocialPolicyConfig_t> policies;
};

} // namespace ac
