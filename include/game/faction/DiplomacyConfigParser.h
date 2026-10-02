#pragma once

#include "game/faction/DiplomacyConfig.h"

#include <string>

namespace ac
{

class DiplomacyConfigParser
{
public:
    // Load config/diplomacy.json. Throws if the file cannot be opened or fails validation.
    DiplomacyConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
