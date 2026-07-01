#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/effects/BonusEffectParser.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

class SocialPolicyConfigParser
{
public:
    SocialPolicyConfigParser();
    ~SocialPolicyConfigParser() = default;

    std::vector<SocialPolicyConfig> ParseConfig(const std::string& configPath);

private:
    SocialPolicyConfig ParsePolicyConfig(const nlohmann::json& policyJson);
    SocialCategory ParseCategory(const std::string& category);
};

} // namespace ac
