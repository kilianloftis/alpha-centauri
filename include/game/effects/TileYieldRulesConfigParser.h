#pragma once

#include "game/effects/TileYieldRulesConfig.h"
#include <string>

namespace ac
{

// Loads config/tile_yield_rules.json — FactionGlobal MaxClamp resource caps (and related)
// effects merged into every faction's effect pool, plus the world-level yield scalars. Parsed
// with EffectSourceKind_t::TileYieldRules so ValidateScopeForSource rejects scopes that can
// never apply (e.g. ThisPop).
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
