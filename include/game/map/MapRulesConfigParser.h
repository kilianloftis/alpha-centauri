#pragma once

#include "game/map/MapRulesConfig.h"

#include <string>

namespace ac
{

class MapRulesConfigParser
{
public:
    // Throws if the file cannot be opened or a required scalar is missing or out of range.
    static MapRulesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
