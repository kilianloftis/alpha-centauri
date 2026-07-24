#pragma once

#include "game/map/WorldGenDecorationConfig.h"
#include <string>

namespace ac
{

class WorldGenDecorationConfigParser
{
public:
    WorldGenDecorationConfigParser() = default;
    ~WorldGenDecorationConfigParser() = default;

    // Load config/worldGen/decoration.json. Throws if the file cannot be opened or parsed.
    WorldGenDecorationConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
