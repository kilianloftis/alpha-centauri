#include "game/ecology/EcoDamageConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "game/effects/TriggeredEffectParser.h"
#include "lib/LuaRuntime.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

void RejectUnknownKeys_(const nlohmann::json& rJson, const std::vector<std::string>& rKnown,
                        const std::string& rContext)
{
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error(rContext + ": unknown key '" + rKey + "'");
        }
    }
}

const nlohmann::json& RequireKey_(const nlohmann::json& rJson, const char* pKey,
                                  const std::string& rContext)
{
    if (!rJson.contains(pKey))
    {
        throw std::runtime_error(rContext + ": missing required '" + pKey + "'");
    }
    return rJson.at(pKey);
}

std::string LoadFormula_(const std::string& rFormulaPath, LuaRuntime& rLua)
{
    sol::protected_function_result result =
        rLua.GetState().safe_script_file(rFormulaPath, sol::script_pass_on_error);
    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load eco damage script '" + rFormulaPath
                                 + "': " + err.what());
    }
    sol::table table = result;
    std::string formula = table.get_or("damage_formula", std::string(""));
    if (formula.empty())
    {
        throw std::runtime_error("eco damage script '" + rFormulaPath
                                 + "' must set damage_formula to a non-empty Lua expression");
    }
    return formula;
}

void ParseFungalPop_(const nlohmann::json& rPop, const std::string& rCtx,
                     EcoDamageConfig_t& rParsed)
{
    if (!rPop.is_object())
    {
        throw std::runtime_error(rCtx + " must be an object");
    }
    RejectUnknownKeys_(rPop, {"max_chance_percent", "on_pop_effects"}, rCtx);

    const nlohmann::json& rMax = RequireKey_(rPop, "max_chance_percent", rCtx);
    if (!rMax.is_number_integer())
    {
        throw std::runtime_error(rCtx + ": 'max_chance_percent' must be an integer");
    }
    rParsed.maxChancePercent = rMax.get<int>();
    if (rParsed.maxChancePercent < 0 || rParsed.maxChancePercent > 100)
    {
        throw std::runtime_error(rCtx + ": 'max_chance_percent' must be 0..100");
    }

    if (!RequireKey_(rPop, "on_pop_effects", rCtx).is_array())
    {
        throw std::runtime_error(rCtx + ": 'on_pop_effects' must be an array");
    }
    rParsed.onPopEffects =
        TriggeredEffectParser::ParseTriggeredEffects(rPop, "on_pop_effects", "eco_damage");
}

EcoDamageConfig_t ParseEcoDamage_(const nlohmann::json& rJson, const std::string& rCtx)
{
    RejectUnknownKeys_(rJson, {"effects", "fungal_pop"}, rCtx);
    EcoDamageConfig_t parsed;
    if (!RequireKey_(rJson, "effects", rCtx).is_array())
    {
        throw std::runtime_error(rCtx + ": 'effects' must be an array");
    }
    parsed.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::EcoDamage, "eco_damage");
    ParseFungalPop_(RequireKey_(rJson, "fungal_pop", rCtx), rCtx + " fungal_pop", parsed);
    return parsed;
}

} // namespace

EcoDamageConfig_t EcoDamageConfigParser::ParseConfig(const std::string& rConfigPath,
                                                     const std::string& rFormulaPath,
                                                     LuaRuntime& rLua)
{
    EcoDamageConfig_t config = JsonConfigLoader::LoadObjectFile<EcoDamageConfig_t>(
        rConfigPath, "eco damage", [&rConfigPath](const nlohmann::json& rJson) {
            return ParseEcoDamage_(rJson, "Eco damage config '" + rConfigPath + "'");
        });
    config.damageFormula = LoadFormula_(rFormulaPath, rLua);
    return config;
}

} // namespace ac
