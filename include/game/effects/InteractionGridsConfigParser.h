#pragma once

#include "game/effects/InteractionGridsConfig.h"

#include <string>

namespace ac
{

class InteractionGridsConfigParser
{
public:
    InteractionGridsConfigParser() = default;

    // Throws if the file cannot be opened, axes/cells are incomplete, or values are unknown.
    InteractionGridsConfig_t ParseConfig(const std::string& configPath) const;
};

} // namespace ac
