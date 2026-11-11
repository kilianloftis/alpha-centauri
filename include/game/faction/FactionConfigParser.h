#pragma once

#include "game/faction/FactionConfig.h"
#include <string>
#include <vector>

namespace ac
{

class FactionConfigParser
{
public:
    FactionConfigParser() = default;
    ~FactionConfigParser() = default;

    std::vector<FactionConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
