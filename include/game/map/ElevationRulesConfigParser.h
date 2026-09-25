#pragma once

#include "game/map/ElevationRulesConfig.h"

#include <string>

namespace ac
{

class ElevationRulesConfigParser
{
public:
    ElevationRulesConfigParser() = default;
    ~ElevationRulesConfigParser() = default;

    // Throws if the file cannot be opened or a required scalar is missing or out of range.
    ElevationRulesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
