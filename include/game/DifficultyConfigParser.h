#pragma once

#include "game/DifficultyConfig.h"

#include <string>

namespace ac
{

class DifficultyConfigParser
{
public:
    // Load config/difficulty.json. Throws on missing file, unknown keys, duplicate ids,
    // missing default, or invalid effects.
    DifficultyConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
