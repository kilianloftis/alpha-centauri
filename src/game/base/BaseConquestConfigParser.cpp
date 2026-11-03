#include "game/base/BaseConquestConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

namespace
{

int RequireInteger_(const nlohmann::json& rObject, const char* pKey, const std::string& rPath)
{
    if (!rObject.contains(pKey))
    {
        throw std::runtime_error("BaseConquestConfigParser: " + rPath + " requires integer '"
                                 + pKey + "'");
    }
    const nlohmann::json& rValue = rObject.at(pKey);
    if (!rValue.is_number_integer())
    {
        throw std::runtime_error("BaseConquestConfigParser: " + rPath + " requires integer '"
                                 + pKey + "'");
    }
    return rValue.get<int>();
}

int RequirePercent_(const nlohmann::json& rObject, const char* pKey, const std::string& rPath)
{
    const int value = RequireInteger_(rObject, pKey, rPath);
    if (value < 0 || value > 100)
    {
        throw std::runtime_error("BaseConquestConfigParser: " + rPath + "." + pKey
                                 + " must be in 0..100");
    }
    return value;
}

int RequireNonNeg_(const nlohmann::json& rObject, const char* pKey, const std::string& rPath)
{
    const int value = RequireInteger_(rObject, pKey, rPath);
    if (value < 0)
    {
        throw std::runtime_error("BaseConquestConfigParser: " + rPath + "." + pKey
                                 + " must be >= 0");
    }
    return value;
}

// The escape score divides by region bases plus this times population, so it must not be 0.
int RequirePositive_(const nlohmann::json& rObject, const char* pKey, const std::string& rPath)
{
    const int value = RequireInteger_(rObject, pKey, rPath);
    if (value <= 0)
    {
        throw std::runtime_error("BaseConquestConfigParser: " + rPath + "." + pKey
                                 + " must be > 0");
    }
    return value;
}

} // namespace

BaseConquestConfig_t BaseConquestConfigParser::ParseConfig(const std::string& configPath) const
{
    std::ifstream file(configPath);
    if (!file)
    {
        throw std::runtime_error("BaseConquestConfigParser: cannot open " + configPath);
    }

    nlohmann::json json;
    file >> json;

    BaseConquestConfig_t config;
    config.effects = EffectConfigParser::ParseEffects(json, EffectSourceKind_t::BaseConquest,
                                                     "base_conquest");
    if (json.contains("escape_colony_pod"))
    {
        const nlohmann::json& rPod = json.at("escape_colony_pod");
        if (rPod.contains("components"))
        {
            config.escapeColonyPod.componentIds =
                rPod.at("components").get<std::vector<std::string>>();
        }
    }

    if (!json.contains("native_raid") || !json.at("native_raid").is_object())
    {
        throw std::runtime_error("BaseConquestConfigParser: requires object 'native_raid'");
    }
    const nlohmann::json& rRaid = json.at("native_raid");
    config.nativeRaid.facilityChancePercent =
        RequirePercent_(rRaid, "facility_chance_percent", "native_raid");
    config.nativeRaid.popLoss = RequireNonNeg_(rRaid, "pop_loss", "native_raid");

    if (!json.contains("headquarters_escape") || !json.at("headquarters_escape").is_object())
    {
        throw std::runtime_error("BaseConquestConfigParser: requires object 'headquarters_escape'");
    }
    const nlohmann::json& rEscape = json.at("headquarters_escape");
    const std::string escapePath = "headquarters_escape";
    HeadquartersEscapeConfig_t& rEscapeConfig = config.headquartersEscape;
    rEscapeConfig.cost = RequireNonNeg_(rEscape, "cost", escapePath);
    rEscapeConfig.scoreSeed = RequireNonNeg_(rEscape, "score_seed", escapePath);
    rEscapeConfig.scoreScale = RequireNonNeg_(rEscape, "score_scale", escapePath);
    rEscapeConfig.otherRegionDistanceFactor =
        RequireNonNeg_(rEscape, "other_region_distance_factor", escapePath);
    rEscapeConfig.populationFactor = RequirePositive_(rEscape, "population_factor", escapePath);

    return config;
}

} // namespace ac
