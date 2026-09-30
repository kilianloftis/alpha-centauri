#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/TriggeredEffect.h"

#include <string>
#include <vector>

namespace ac
{

class LuaRuntime;

// config/eco_damage.json plus the formula config/eco_damage.lua returns.
struct EcoDamageConfig_t
{
    // Continuous faction-wide baselines (the clean-minerals cap). Enter every faction pool.
    std::vector<EffectConfig_t> effects;
    // Ceiling on the per-base fungal-pop roll, in percent.
    int maxChancePercent = 0;
    // Applied at a base whose roll hit, with base, owner and pop tile stamped.
    std::vector<TriggeredEffectConfig_t> onPopEffects;
    // Lua expression evaluating to the eco-damage percentage.
    std::string damageFormula;
};

class EcoDamageConfigParser
{
public:
    // Every key in rConfigPath is required. rFormulaPath is a Lua script returning a table
    // whose damage_formula names a non-empty expression.
    EcoDamageConfig_t ParseConfig(const std::string& rConfigPath,
                                  const std::string& rFormulaPath, LuaRuntime& rLua);
};

} // namespace ac
