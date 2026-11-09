#include "game/map/MapRulesConfigParser.h"

#include "game/effects/EffectConfigParser.h"

#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

int RequireWholeMeters_(const nlohmann::json& rJson, const char* pKey)
{
    const double value = EffectConfigParser::RequireNumber(rJson, pKey);
    if (value != std::floor(value))
    {
        throw std::runtime_error(std::string("map_rules '") + pKey
                                 + "' must be a whole number of meters");
    }
    return static_cast<int>(value);
}

int RequireCount_(const nlohmann::json& rJson, const char* pKey)
{
    if (!rJson.contains(pKey) || !rJson.at(pKey).is_number_integer())
    {
        throw std::runtime_error(std::string("map_rules '") + pKey + "' must be an integer");
    }
    return rJson.at(pKey).get<int>();
}

void ValidateElevationRules_(const MapRulesConfig_t& rConfig)
{
    if (rConfig.oceanShelfMeters >= rConfig.oceanLevelMeters)
    {
        throw std::runtime_error(
            "map_rules 'ocean_shelf_meters' must be < 'ocean_level_meters', got "
            + std::to_string(rConfig.oceanShelfMeters) + " >= "
            + std::to_string(rConfig.oceanLevelMeters));
    }
    if (rConfig.levelMinMeters <= 0)
    {
        throw std::runtime_error("map_rules 'level_min_meters' must be > 0, got "
                                 + std::to_string(rConfig.levelMinMeters));
    }
    if (rConfig.levelMaxMeters < rConfig.levelMinMeters)
    {
        throw std::runtime_error(
            "map_rules 'level_max_meters' must be >= 'level_min_meters', got "
            + std::to_string(rConfig.levelMaxMeters) + " < "
            + std::to_string(rConfig.levelMinMeters));
    }
    if (rConfig.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::runtime_error(
            "map_rules 'max_adjacent_difference_meters' must be > 0, got "
            + std::to_string(rConfig.maxAdjacentDifferenceMeters));
    }
    if (rConfig.referenceLevelMeters <= 0)
    {
        throw std::runtime_error("map_rules 'reference_level_meters' must be > 0, got "
                                 + std::to_string(rConfig.referenceLevelMeters));
    }
    if (rConfig.spreadAltitudeLimitMeters < rConfig.oceanLevelMeters)
    {
        throw std::runtime_error(
            "map_rules 'spread_altitude_limit_meters' must be >= 'ocean_level_meters', got "
            + std::to_string(rConfig.spreadAltitudeLimitMeters) + " < "
            + std::to_string(rConfig.oceanLevelMeters));
    }
    if (rConfig.missingHeadquartersDistance < 1)
    {
        throw std::runtime_error("map_rules 'missing_headquarters_distance' must be >= 1, got "
                                 + std::to_string(rConfig.missingHeadquartersDistance));
    }
}

} // namespace

MapRulesConfig_t MapRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open map rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);

    MapRulesConfig_t config;
    config.oceanLevelMeters = RequireWholeMeters_(json, "ocean_level_meters");
    config.oceanShelfMeters = RequireWholeMeters_(json, "ocean_shelf_meters");
    config.levelMinMeters = RequireWholeMeters_(json, "level_min_meters");
    config.levelMaxMeters = RequireWholeMeters_(json, "level_max_meters");
    config.maxAdjacentDifferenceMeters = RequireWholeMeters_(json, "max_adjacent_difference_meters");
    config.referenceLevelMeters = RequireWholeMeters_(json, "reference_level_meters");
    config.spreadAltitudeLimitMeters =
        RequireWholeMeters_(json, "spread_altitude_limit_meters");
    config.missingHeadquartersDistance = RequireCount_(json, "missing_headquarters_distance");

    ValidateElevationRules_(config);
    return config;
}

} // namespace ac
