#pragma once

#include "game/effects/TileYieldRulesConfig.h"
#include <string>

namespace ac
{

// Loads config/tile_yield_rules.json as EffectSourceKind_t::TileYieldRules.
class TileYieldRulesConfigParser
{
public:
    TileYieldRulesConfigParser() = default;
    ~TileYieldRulesConfigParser() = default;

    // Throws if the file cannot be opened, the effects array is invalid, or a required
    // scalar is missing or out of range — no C++ default silently stands in for a key.
    TileYieldRulesConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
