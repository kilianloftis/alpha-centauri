#pragma once

#include "game/effects/InteractionGridsConfig.h"

#include <string>

namespace ac
{

class InteractionGridsConfigParser
{
public:
    // Throws if the file cannot be opened, axes/cells are incomplete, or values are unknown.
    static InteractionGridsConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
