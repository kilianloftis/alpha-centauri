#pragma once

#include "game/combat/CombatConfig.h"

#include <string>

namespace ac
{

class CombatConfigParser
{
public:
    // Load config/combat.json. Throws if the file cannot be opened or fails validation.
    CombatConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
