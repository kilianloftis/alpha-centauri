#pragma once

#include "game/effects/EffectConfig.h"
#include <string>
#include <vector>

namespace ac
{

// Loads config/police_rules.json — FactionUnits baselines for combat units (away-from-home
// weight and police effectiveness), merged into every faction's effect pool.
class PoliceRulesConfigParser
{
public:
    PoliceRulesConfigParser() = default;
    ~PoliceRulesConfigParser() = default;

    // Throws if the file cannot be opened or the effects array is invalid.
    std::vector<EffectConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
