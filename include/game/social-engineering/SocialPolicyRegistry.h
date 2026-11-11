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
    // social_policies.json is an object (upheaval + policies); only the policy list is stored.
    void Load(const std::string& rConfigPath)
    {
        Assign(SocialPolicyConfigParser::ParseConfig(rConfigPath).policies);
    }

    // Exactly one default per category, checked at load so a bad mod file is named.
    void Validate_() override
    {
        for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
        {
            GetDefaultForCategory(category);
        }
    }

    // The category's `default: true` policy; Validate_ guarantees one.
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
