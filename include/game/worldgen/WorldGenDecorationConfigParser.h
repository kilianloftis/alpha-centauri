#pragma once

#include "game/worldgen/WorldGenDecorationConfig.h"
#include <string>

namespace ac
{

class WorldGenDecorationConfigParser
{
public:
    // Load config/worldGen/decoration.json. Throws if the file cannot be opened or parsed.
    static WorldGenDecorationConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
