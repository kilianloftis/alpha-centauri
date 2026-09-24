#pragma once

#include "game/effects/EffectConfig.h"
#include <string>
#include <vector>

namespace ac
{

// Loads config/world_rules.json — standing WorldGlobal effects appended once per session.
class WorldRulesConfigParser
{
public:
    WorldRulesConfigParser() = default;
    ~WorldRulesConfigParser() = default;

    // Throws if the file cannot be opened or the effects array is invalid.
    std::vector<EffectConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
