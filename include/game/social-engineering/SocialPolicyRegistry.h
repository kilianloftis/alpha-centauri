#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/Registry.h"
#include <vector>

namespace ac
{

class SocialPolicyRegistry : public Registry<SocialPolicyConfig_t, SocialPolicyConfigParser>
{
public:
    // All policies belonging to a specific category.
    std::vector<const SocialPolicyConfig_t*> GetByCategory(SocialCategory_t rCategory) const
    {
        std::vector<const SocialPolicyConfig_t*> result;
        for (const auto& config : m_configs)
        {
            if (config.category == rCategory)
            {
                result.push_back(&config);
            }
        }
        return result;
    }
};

} // namespace ac
