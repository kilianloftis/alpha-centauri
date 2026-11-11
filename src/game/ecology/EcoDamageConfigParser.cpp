#include "game/ecology/EcoDamageConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/config/LuaConfigLoader.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

void ParseFungalPop_(const ConfigReader& rReader, EcoDamageConfig_t& rParsed)
{
    rReader.RejectUnknownKeys({"max_chance_percent", "on_pop_effects"});
    rParsed.maxChancePercent = rReader.IntInRange("max_chance_percent", 0, 100);
    rReader.Array("on_pop_effects");
    rParsed.onPopEffects = TriggeredEffectParser::ParseTriggeredEffects(
        rReader.Json(), "on_pop_effects", "eco_damage");
}

EcoDamageConfig_t ParseEcoDamage_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"effects", "fungal_pop"});
    EcoDamageConfig_t parsed;
    rReader.Array("effects");
    parsed.effects = EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::EcoDamage,
                                                      "eco_damage");
    ParseFungalPop_(rReader.Object("fungal_pop"), parsed);
    return parsed;
}

} // namespace

EcoDamageConfig_t EcoDamageConfigParser::ParseConfig(const std::string& rConfigPath,
                                                     const std::string& rFormulaPath,
                                                     LuaRuntime& rLua)
{
    EcoDamageConfig_t config = JsonConfigLoader::LoadObjectFile<EcoDamageConfig_t>(
        rConfigPath, "eco damage", ParseEcoDamage_);
    const sol::table formulas = LuaConfigLoader::RunScript(rLua, rFormulaPath, "eco damage");
    config.damageFormula =
        LuaConfigLoader::RequireFormula(formulas, "damage_formula", rFormulaPath, "eco damage");
    return config;
}

} // namespace ac
