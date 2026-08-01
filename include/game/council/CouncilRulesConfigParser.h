#pragma once

#include "game/council/CouncilRulesConfig.h"

#include <string>

namespace ac
{

class CouncilRulesConfigParser
{
public:
    // Load config/council/rules.json. Throws if the file cannot be opened or fails validation.
    CouncilRulesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
