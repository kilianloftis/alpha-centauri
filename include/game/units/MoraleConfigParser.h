#pragma once

#include "game/units/MoraleConfig.h"
#include <string>

namespace ac
{

class MoraleConfigParser
{
public:
    MoraleConfigParser() = default;

    // Load morale_levels.json. Throws if the file cannot be opened or fails validation.
    MoraleConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
