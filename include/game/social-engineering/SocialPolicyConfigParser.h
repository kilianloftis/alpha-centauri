#pragma once

#include "game/social-engineering/SocialPoliciesConfig.h"
#include <string>

namespace ac
{

class SocialPolicyConfigParser
{
public:
    SocialPolicyConfigParser() = default;
    ~SocialPolicyConfigParser() = default;

    SocialPoliciesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
