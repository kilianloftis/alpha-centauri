#pragma once

#include "game/social-engineering/SocialPoliciesConfig.h"
#include <string>

namespace ac
{

class SocialPolicyConfigParser
{
public:
    static SocialPoliciesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
