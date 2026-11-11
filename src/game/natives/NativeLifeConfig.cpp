#include "game/natives/NativeLifeConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/LuaRuntime.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

int RequireWholeCount_(const nlohmann::json& rJson, const char* pKey, const std::string& rCtx)
{
    if (!rJson.contains(pKey))
    {
        throw std::runtime_error(rCtx + ": missing required '" + pKey + "'");
    }
    const double value = EffectConfigParser::RequireNumber(rJson, pKey);
    if (value != std::floor(value))
    {
        throw std::runtime_error(rCtx + ": '" + pKey + "' must be a whole number");
    }
    return static_cast<int>(value);
}

int RequireNonNegativeInt_(const nlohmann::json& rJson, const char* pKey, const std::string& rCtx)
{
    const int value = RequireWholeCount_(rJson, pKey, rCtx);
    if (value < 0)
    {
        throw std::runtime_error(rCtx + ": '" + pKey + "' must be >= 0, got "
                                 + std::to_string(value));
    }
    return value;
}

std::array<int, 6> RequireLifecycleThresholds_(const nlohmann::json& rJson,
                                               const std::string& rCtx)
{
    if (!rJson.contains("planet_lifecycle_turn_thresholds")
        || !rJson.at("planet_lifecycle_turn_thresholds").is_array())
    {
        throw std::runtime_error(rCtx
                                 + ": 'planet_lifecycle_turn_thresholds' must be an array of 6 "
                                   "integers");
    }
    const nlohmann::json& rArr = rJson.at("planet_lifecycle_turn_thresholds");
    if (rArr.size() != 6)
    {
        throw std::runtime_error(
            rCtx + ": 'planet_lifecycle_turn_thresholds' must have exactly 6 entries");
    }
    std::array<int, 6> thresholds{};
    for (size_t i = 0; i < 6; ++i)
    {
        if (!rArr.at(i).is_number_integer())
        {
            throw std::runtime_error(rCtx
                                     + ": 'planet_lifecycle_turn_thresholds' entries must be "
                                       "integers");
        }
        const int value = rArr.at(i).get<int>();
        if (value < 0)
        {
            throw std::runtime_error(
                rCtx + ": 'planet_lifecycle_turn_thresholds' entries must be >= 0");
        }
        thresholds[i] = value;
    }
    return thresholds;
}

NativeLifeLevel_t ParseLevel_(const nlohmann::json& rJson)
{
    if (!rJson.is_object())
    {
        throw std::runtime_error("native life level entry must be a JSON object");
    }
    ConfigFields::RejectUnknownKeys(rJson, {"id", "name", "fauna_forms", "effects"},
                                    "native life level");
    if (!rJson.contains("id") || !rJson.at("id").is_string()
        || rJson.at("id").get<std::string>().empty())
    {
        throw std::runtime_error("native life level missing non-empty string 'id'");
    }
    NativeLifeLevel_t level;
    level.id = rJson.at("id").get<std::string>();
    const std::string ctx = "native life level '" + level.id + "'";
    if (!rJson.contains("name") || !rJson.at("name").is_string())
    {
        throw std::runtime_error(ctx + ": 'name' must be a string");
    }
    level.name = rJson.at("name").get<std::string>();
    if (!rJson.contains("fauna_forms") || !rJson.at("fauna_forms").is_number_integer())
    {
        throw std::runtime_error(ctx + ": 'fauna_forms' must be an integer");
    }
    level.faunaForms = rJson.at("fauna_forms").get<int>();
    if (level.faunaForms < 0 || level.faunaForms > 2)
    {
        throw std::runtime_error(ctx + ": 'fauna_forms' must be 0..2");
    }
    if (!rJson.contains("effects") || !rJson.at("effects").is_array())
    {
        throw std::runtime_error(ctx + ": 'effects' must be an array");
    }
    level.effects =
        EffectConfigParser::ParseEffects(rJson, EffectSourceKind_t::NativeLifeLevel, level.id);
    return level;
}

void LoadFormulas_(const std::string& rFormulaPath, LuaRuntime& rLua, NativeLifeConfig_t& rConfig)
{
    sol::protected_function_result result =
        rLua.GetState().safe_script_file(rFormulaPath, sol::script_pass_on_error);
    if (!result.valid())
    {
        const sol::error err = result;
        throw std::runtime_error("Failed to load native life script '" + rFormulaPath + "': "
                                 + err.what());
    }
    sol::table table = result;
    auto requireFormula = [&](const char* pKey) -> std::string
    {
        std::string formula = table.get_or(pKey, std::string(""));
        if (formula.empty())
        {
            throw std::runtime_error("native life script '" + rFormulaPath + "' must set " + pKey
                                     + " to a non-empty Lua expression");
        }
        return formula;
    };
    rConfig.boatPeriodFormula = requireFormula("boat_period_formula");
    rConfig.earlyLimitThresholdFormula = requireFormula("early_limit_threshold_formula");
    rConfig.boatCargoFormula = requireFormula("boat_cargo_formula");
    rConfig.ambushAttemptsFormula = requireFormula("ambush_attempts_formula");
    rConfig.swarmYearThresholdFormula = requireFormula("swarm_year_threshold_formula");
}

NativeLifeConfig_t ParseNativeLifeJson_(const nlohmann::json& rJson, const std::string& rCtx)
{
    static const std::vector<std::string> known = {
        "fungal_bloom_native_lifeforms_min",
        "fungal_bloom_native_lifeforms_max",
        "planet_lifecycle_turn_thresholds",
        "ambush_near_base_min_dist",
        "ambush_near_base_max_dist",
        "boat_empty_ocean_min_region_tiles",
        "default",
        "levels",
    };
    ConfigFields::RejectUnknownKeys(rJson, known, rCtx);

    NativeLifeConfig_t config;
    config.fungalBloomNativeLifeformsMin =
        RequireWholeCount_(rJson, "fungal_bloom_native_lifeforms_min", rCtx);
    config.fungalBloomNativeLifeformsMax =
        RequireWholeCount_(rJson, "fungal_bloom_native_lifeforms_max", rCtx);
    if (config.fungalBloomNativeLifeformsMin < 0)
    {
        throw std::runtime_error(rCtx + ": 'fungal_bloom_native_lifeforms_min' must be >= 0");
    }
    if (config.fungalBloomNativeLifeformsMax < config.fungalBloomNativeLifeformsMin)
    {
        throw std::runtime_error(
            rCtx + ": 'fungal_bloom_native_lifeforms_max' must be >= "
                   "'fungal_bloom_native_lifeforms_min'");
    }
    config.planetLifecycleTurnThresholds = RequireLifecycleThresholds_(rJson, rCtx);
    config.ambushNearBaseMinDist =
        RequireNonNegativeInt_(rJson, "ambush_near_base_min_dist", rCtx);
    config.ambushNearBaseMaxDist =
        RequireNonNegativeInt_(rJson, "ambush_near_base_max_dist", rCtx);
    if (config.ambushNearBaseMaxDist < config.ambushNearBaseMinDist)
    {
        throw std::runtime_error(
            rCtx + ": 'ambush_near_base_max_dist' must be >= 'ambush_near_base_min_dist'");
    }
    config.boatEmptyOceanMinRegionTiles =
        RequireNonNegativeInt_(rJson, "boat_empty_ocean_min_region_tiles", rCtx);
    // Level set only — do not call ParseLevelSet (it rejects sibling session keys).
    if (!rJson.contains("default") || !rJson.at("default").is_string()
        || rJson.at("default").get<std::string>().empty())
    {
        throw std::runtime_error(rCtx + ": 'default' must be a non-empty string");
    }
    if (!rJson.contains("levels") || !rJson.at("levels").is_array() || rJson.at("levels").empty())
    {
        throw std::runtime_error(rCtx + ": 'levels' must be a non-empty array");
    }
    config.defaultId = rJson.at("default").get<std::string>();
    std::unordered_set<std::string> seenIds;
    for (const nlohmann::json& rLevelJson : rJson.at("levels"))
    {
        NativeLifeLevel_t level = ParseLevel_(rLevelJson);
        if (!seenIds.insert(level.id).second)
        {
            throw std::runtime_error(rCtx + ": duplicate level id '" + level.id + "'");
        }
        config.levels.push_back(std::move(level));
    }
    if (!seenIds.contains(config.defaultId))
    {
        throw std::runtime_error(rCtx + ": default '" + config.defaultId
                                 + "' does not match any level id");
    }
    return config;
}

} // namespace

const NativeLifeLevel_t* NativeLifeConfig_t::FindById(const std::string& rId) const
{
    for (const NativeLifeLevel_t& rLevel : levels)
    {
        if (rLevel.id == rId)
        {
            return &rLevel;
        }
    }
    return nullptr;
}

const NativeLifeLevel_t& NativeLifeConfig_t::RequireForSession(const std::string& rLevelId) const
{
    const std::string& rWanted = rLevelId.empty() ? defaultId : rLevelId;
    if (const NativeLifeLevel_t* pLevel = FindById(rWanted))
    {
        return *pLevel;
    }
    throw std::runtime_error("Unknown native life level id '" + rWanted + "'");
}

NativeLifeConfig_t NativeLifeConfigParser::ParseConfig(const std::string& rConfigPath,
                                                       const std::string& rFormulaPath,
                                                       LuaRuntime& rLua)
{
    NativeLifeConfig_t config = JsonConfigLoader::LoadObjectFile<NativeLifeConfig_t>(
        rConfigPath, "native life", [&rConfigPath](const nlohmann::json& rJson) {
            return ParseNativeLifeJson_(rJson, "Native life config '" + rConfigPath + "'");
        });
    LoadFormulas_(rFormulaPath, rLua, config);
    return config;
}

} // namespace ac
