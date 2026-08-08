#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/social-engineering/SocialPolicyConfigParser.h"
#include "lib/Registry.h"
#include <magic_enum.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

// Category name for diagnostics; the wire form is snake_case but the enumerator reads fine here.
inline std::string CategoryName(SocialCategory_t category)
{
    return std::string(magic_enum::enum_name(category));
}

class SocialPolicyRegistry : public Registry<SocialPolicyConfig_t, SocialPolicyConfigParser>
{
public:
    // The starting policy for a category, from the config's `default: true` flag. Throws when a
    // category has no default or more than one: a faction cannot be constructed without one, and
    // the failure belongs at config load naming the category, not at every faction constructor.
    const SocialPolicyConfig_t& GetDefaultForCategory(SocialCategory_t category) const
    {
        const SocialPolicyConfig_t* pDefault = nullptr;
        for (const SocialPolicyConfig_t& rConfig : m_configs)
        {
            if (rConfig.category != category || !rConfig.bDefault)
            {
                continue;
            }
            if (pDefault)
            {
                throw std::runtime_error(
                    "social_policies: category '" + CategoryName(category)
                    + "' has more than one default ('" + pDefault->id + "' and '" + rConfig.id
                    + "')");
            }
            pDefault = &rConfig;
        }
        if (!pDefault)
        {
            throw std::runtime_error("social_policies: category '" + CategoryName(category)
                                     + "' has no policy marked \"default\": true");
        }
        return *pDefault;
    }

    // All policies belonging to a specific category.
    std::vector<const SocialPolicyConfig_t*> GetByCategory(SocialCategory_t category) const
    {
        std::vector<const SocialPolicyConfig_t*> result;
        for (const auto& config : m_configs)
        {
            if (config.category == category)
            {
                result.push_back(&config);
            }
        }
        return result;
    }
};

} // namespace ac
