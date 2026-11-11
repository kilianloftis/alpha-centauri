#include "game/natives/NativeLifeConfig.h"

#include "game/effects/EffectConfigParser.h"
#include "lib/config/ConfigSets.h"
#include "lib/config/JsonConfigLoader.h"
#include "lib/config/LuaConfigLoader.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

constexpr const char* k_ScriptNoun = "native life";

int RequireWholeCount_(const ConfigReader& rReader, const char* pKey)
{
    const double value = rReader.Number(pKey);
    if (value != std::floor(value))
    {
        rReader.FailKey(pKey, "must be a whole number");
    }
    return static_cast<int>(value);
}

int RequireNonNegativeInt_(const ConfigReader& rReader, const char* pKey)
{
    const int value = RequireWholeCount_(rReader, pKey);
    if (value < 0)
    {
        rReader.FailKey(pKey, "must be >= 0, got " + std::to_string(value));
    }
    return value;
}

void RequireNotBelow_(const ConfigReader& rReader, const char* pKey, int value,
                      const char* pFloorKey, int floor)
{
    if (value < floor)
    {
        rReader.FailKey(pKey, std::string("must be >= '") + pFloorKey + "'");
    }
}

std::array<int, 6> RequireLifecycleThresholds_(const ConfigReader& rReader)
{
    const char* pKey = "planet_lifecycle_turn_thresholds";
    const nlohmann::json& rArr = rReader.Array(pKey);
    if (rArr.size() != 6)
    {
        rReader.FailKey(pKey, "must have exactly 6 entries");
    }
    std::array<int, 6> thresholds{};
    for (size_t i = 0; i < 6; ++i)
    {
        if (!rArr.at(i).is_number_integer() || rArr.at(i).get<int>() < 0)
        {
            rReader.FailKey(pKey, "entries must be integers >= 0");
        }
        thresholds[i] = rArr.at(i).get<int>();
    }
    return thresholds;
}

NativeLifeLevel_t ParseLevel_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"id", "name", "fauna_forms", "fungus_coverage_multiplier",
                               "effects"});
    NativeLifeLevel_t level;
    level.id = rReader.NonEmptyString("id");
    level.name = rReader.String("name");
    level.faunaForms = rReader.IntInRange("fauna_forms", 0, 2);
    level.fungusCoverageMultiplier =
        static_cast<float>(rReader.NumberAtLeast("fungus_coverage_multiplier", 0.0));
    rReader.Array("effects");
    level.effects = EffectConfigParser::ParseEffects(rReader.Json(),
                                                     EffectSourceKind_t::NativeLifeLevel, level.id);
    return level;
}

void LoadFormulas_(const std::string& rFormulaPath, LuaRuntime& rLua, NativeLifeConfig_t& rConfig)
{
    const sol::table table = LuaConfigLoader::RunScript(rLua, rFormulaPath, k_ScriptNoun);
    const auto requireFormula = [&](const char* pKey) {
        return LuaConfigLoader::RequireFormula(table, pKey, rFormulaPath, k_ScriptNoun);
    };
    rConfig.boatPeriodFormula = requireFormula("boat_period_formula");
    rConfig.earlyLimitThresholdFormula = requireFormula("early_limit_threshold_formula");
    rConfig.boatCargoFormula = requireFormula("boat_cargo_formula");
    rConfig.ambushAttemptsFormula = requireFormula("ambush_attempts_formula");
    rConfig.swarmYearThresholdFormula = requireFormula("swarm_year_threshold_formula");
}

NativeLifeConfig_t ParseNativeLifeJson_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "fungal_bloom_native_lifeforms_min",
        "fungal_bloom_native_lifeforms_max",
        "planet_lifecycle_turn_thresholds",
        "ambush_near_base_min_dist",
        "ambush_near_base_max_dist",
        "boat_empty_ocean_min_region_tiles",
        "default",
        "levels",
    });

    NativeLifeConfig_t config;
    config.fungalBloomNativeLifeformsMin =
        RequireNonNegativeInt_(rReader, "fungal_bloom_native_lifeforms_min");
    config.fungalBloomNativeLifeformsMax =
        RequireWholeCount_(rReader, "fungal_bloom_native_lifeforms_max");
    RequireNotBelow_(rReader, "fungal_bloom_native_lifeforms_max",
                     config.fungalBloomNativeLifeformsMax, "fungal_bloom_native_lifeforms_min",
                     config.fungalBloomNativeLifeformsMin);
    config.planetLifecycleTurnThresholds = RequireLifecycleThresholds_(rReader);
    config.ambushNearBaseMinDist = RequireNonNegativeInt_(rReader, "ambush_near_base_min_dist");
    config.ambushNearBaseMaxDist = RequireNonNegativeInt_(rReader, "ambush_near_base_max_dist");
    RequireNotBelow_(rReader, "ambush_near_base_max_dist", config.ambushNearBaseMaxDist,
                     "ambush_near_base_min_dist", config.ambushNearBaseMinDist);
    config.boatEmptyOceanMinRegionTiles =
        RequireNonNegativeInt_(rReader, "boat_empty_ocean_min_region_tiles");
    ConfigSets::LevelSet_t<NativeLifeLevel_t> set =
        ConfigSets::ParseLevelSet<NativeLifeLevel_t>(rReader, ParseLevel_);
    config.defaultId = std::move(set.defaultId);
    config.levels = std::move(set.levels);
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
        rConfigPath, "native life", ParseNativeLifeJson_);
    LoadFormulas_(rFormulaPath, rLua, config);
    return config;
}

} // namespace ac
