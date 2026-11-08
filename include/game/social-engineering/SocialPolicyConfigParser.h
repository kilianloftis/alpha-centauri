#pragma once

#include "game/social-engineering/SocialPoliciesConfig.h"
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

class SocialPolicyConfigParser
{
public:
    SocialPolicyConfigParser() = default;
    ~SocialPolicyConfigParser() = default;

    SocialPoliciesConfig_t ParseConfig(const std::string& configPath);

private:
    UpheavalCostConfig_t ParseUpheaval_(const nlohmann::json& rUpheavalJson) const;
    SocialPolicyConfig_t ParsePolicyConfig_(const nlohmann::json& policyJson);
    SocialCategory_t ParseCategory_(const std::string& category);
};

} // namespace ac
