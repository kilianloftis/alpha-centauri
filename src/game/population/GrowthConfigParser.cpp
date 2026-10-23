#include "game/population/GrowthConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

namespace
{

[[noreturn]] void FailGrowth_(const std::string& rConfigPath, const std::string& rMessage)
{
    throw std::runtime_error("Growth config '" + rConfigPath + "': " + rMessage);
}

// A positive Add on starting_size or max_base_size. Returns the stat it sets.
StatId_t ValidateBaselineEffect_(const EffectConfig_t& rEffect, const std::string& rConfigPath)
{
    const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (!pMod)
    {
        FailGrowth_(rConfigPath, "only StatModifier effects are allowed");
    }
    if (pMod->stat != StatId_t::StartingSize && pMod->stat != StatId_t::MaxBaseSize)
    {
        FailGrowth_(rConfigPath, "stat is not allowed here (use starting_size or max_base_size)");
    }
    if (pMod->op != ModifierOp_t::Add)
    {
        FailGrowth_(rConfigPath, "population baselines must use op Add");
    }
    if (pMod->amount <= 0.0)
    {
        FailGrowth_(rConfigPath,
                    "population baselines must be > 0, got " + std::to_string(pMod->amount));
    }
    return pMod->stat;
}

// Exactly the two population baselines, so nothing joins the faction pool from a file whose
// name does not say so.
void ValidateGrowthEffects_(const std::vector<EffectConfig_t>& rEffects,
                            const std::string& rConfigPath)
{
    bool bHasStartingSize = false;
    bool bHasMaxBaseSize = false;
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const StatId_t stat = ValidateBaselineEffect_(rEffect, rConfigPath);
        bHasStartingSize = bHasStartingSize || stat == StatId_t::StartingSize;
        bHasMaxBaseSize = bHasMaxBaseSize || stat == StatId_t::MaxBaseSize;
    }
    // Absent, StartingSize 0 throws at the first founding and MaxBaseSize 0 stops all growth, so
    // both are required at load.
    if (!bHasStartingSize)
    {
        FailGrowth_(rConfigPath, "'effects' must contain a starting_size StatModifier");
    }
    if (!bHasMaxBaseSize)
    {
        FailGrowth_(rConfigPath, "'effects' must contain a max_base_size StatModifier");
    }
}

// Required: a nutrients_per_pop of 0 would make every base grow every turn.
int ReadPositive_(const nlohmann::json& rJson, const char* pKey, const std::string& rConfigPath)
{
    const std::string key = std::string("'") + pKey + "' ";
    if (!rJson.contains(pKey))
    {
        FailGrowth_(rConfigPath, key + "is required");
    }
    if (!rJson.at(pKey).is_number_integer())
    {
        FailGrowth_(rConfigPath, key + "must be an integer");
    }
    const int value = rJson.at(pKey).get<int>();
    if (value <= 0)
    {
        FailGrowth_(rConfigPath, key + "must be > 0, got " + std::to_string(value));
    }
    return value;
}

GrowthConfig_t ParseGrowth_(const nlohmann::json& rJson, const std::string& rConfigPath)
{
    GrowthConfig_t config;
    config.nutrientsPerPop = ReadPositive_(rJson, "nutrients_per_pop", rConfigPath);
    config.nutrientIntakePerCitizen =
        ReadPositive_(rJson, "nutrient_intake_per_citizen", rConfigPath);
    if (!rJson.contains("effects") || !rJson.at("effects").is_array())
    {
        FailGrowth_(rConfigPath, "'effects' is required and must be an array");
    }
    // Assigned, not appended: the struct's programmatic defaults exist for tests, and
    // must not survive into a config that declared its own baselines.
    config.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::Growth, "pop_growth");
    ValidateGrowthEffects_(config.effects, rConfigPath);
    return config;
}

} // namespace

GrowthConfig_t GrowthConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<GrowthConfig_t>(
        configPath, "population growth", [&configPath](const nlohmann::json& rJson) {
            return ParseGrowth_(rJson, configPath);
        });
}

} // namespace ac
