#pragma once

#include "game/faction/CommerceConfig.h"

#include <string>

namespace ac
{

class CommerceConfigParser
{
public:
    CommerceConfigParser() = default;
    ~CommerceConfigParser() = default;

    // Load commerce.json. Throws if the file cannot be opened or parsed, if a required key
    // is missing, or if a multiplier is non-finite or not strictly positive.
    CommerceConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
