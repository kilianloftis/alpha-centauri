#pragma once

#include "game/combat/MoraleConfig.h"
#include <string>

namespace ac
{

class MoraleConfigParser
{
public:
    // Load morale_levels.json. Throws if the file cannot be opened or fails validation.
    static MoraleConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
