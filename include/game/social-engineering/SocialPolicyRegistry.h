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
    // Exactly one default per category, checked at load. Deferring it to the first faction
    // constructor meant a mod's policy file loaded clean and then threw from deep inside
    // session setup, naming a category rather than the file that was wrong.
    void Validate_() override
    {
        for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
        {
            GetDefaultForCategory(category);
        }
    }

    // The starting policy for a category, from the config's `default: true` flag. Validate_
    // has already rejected a file that does not declare exactly one per category, so by the
    // time anyone calls this the answer exists; it still throws rather than return null so a
    // registry built some other way cannot silently hand back nothing.
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
