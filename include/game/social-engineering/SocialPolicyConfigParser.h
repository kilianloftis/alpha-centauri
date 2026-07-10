#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/effects/BonusEffectParser.h"
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

    std::vector<SocialPolicyConfig_t> ParseConfig(const std::string& configPath);

private:
    SocialPolicyConfig_t ParsePolicyConfig(const nlohmann::json& policyJson);
    SocialCategory_t ParseCategory(const std::string& category);
};

} // namespace ac
