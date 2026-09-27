#pragma once

#include "game/atrocities/AtrocityConfig.h"

#include <string>

namespace ac
{

class AtrocityConfigParser
{
public:
    // Load config/atrocities.json. Throws if the file cannot be opened or fails validation.
    AtrocitiesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
