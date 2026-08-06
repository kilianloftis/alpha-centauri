#pragma once

#include "game/effects/BonusEffect.h"
#include <string>
#include <vector>

namespace ac
{

// Loads config/tile_yield_rules.json — FactionGlobal TileResourceCap (and related) effects
// merged into every faction's effect pool. Parsed with EffectSourceKind_t::TileYieldRules
// so ValidateScopeForSource rejects scopes that can never apply (e.g. ThisPop).
class TileYieldRulesConfigParser
{
public:
    TileYieldRulesConfigParser() = default;
    ~TileYieldRulesConfigParser() = default;

    // Throws if the file cannot be opened or the effects array is invalid.
    std::vector<EffectConfig_t> ParseConfig(const std::string& configPath);
};

} // namespace ac
