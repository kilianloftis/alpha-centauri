#include "game/map/ElevationRulesConfigParser.h"

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

} // namespace

ElevationRulesConfig_t ElevationRulesConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open map rules '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);

    ElevationRulesConfig_t config;
    config.minElevationMeters = RequireWholeMeters_(json, "min_elevation_meters");
    config.maxElevationMeters = RequireWholeMeters_(json, "max_elevation_meters");
    config.oceanLevelMeters = RequireWholeMeters_(json, "ocean_level_meters");
    config.oceanShelfMeters = RequireWholeMeters_(json, "ocean_shelf_meters");
    config.levelMinMeters = RequireWholeMeters_(json, "level_min_meters");
    config.levelMaxMeters = RequireWholeMeters_(json, "level_max_meters");
    config.maxAdjacentDifferenceMeters = RequireWholeMeters_(json, "max_adjacent_difference_meters");
    config.referenceLevelMeters = RequireWholeMeters_(json, "reference_level_meters");
    config.spreadAltitudeLimitMeters =
        RequireWholeMeters_(json, "spread_altitude_limit_meters");

    if (config.minElevationMeters >= config.maxElevationMeters)
    {
        throw std::runtime_error(
            "map_rules 'min_elevation_meters' must be < 'max_elevation_meters', got "
            + std::to_string(config.minElevationMeters) + " >= "
            + std::to_string(config.maxElevationMeters));
    }
    if (config.oceanLevelMeters <= config.minElevationMeters
        || config.oceanLevelMeters > config.maxElevationMeters)
    {
        throw std::runtime_error(
            "map_rules 'ocean_level_meters' must be inside ("
            + std::to_string(config.minElevationMeters) + ", "
            + std::to_string(config.maxElevationMeters) + "], got "
            + std::to_string(config.oceanLevelMeters));
    }
    if (config.oceanShelfMeters < config.minElevationMeters
        || config.oceanShelfMeters >= config.oceanLevelMeters)
    {
        throw std::runtime_error(
            "map_rules 'ocean_shelf_meters' must be inside ["
            + std::to_string(config.minElevationMeters) + ", "
            + std::to_string(config.oceanLevelMeters) + "), got "
            + std::to_string(config.oceanShelfMeters));
    }
    if (config.levelMinMeters <= 0)
    {
        throw std::runtime_error("map_rules 'level_min_meters' must be > 0, got "
                                 + std::to_string(config.levelMinMeters));
    }
    if (config.levelMaxMeters < config.levelMinMeters)
    {
        throw std::runtime_error(
            "map_rules 'level_max_meters' must be >= 'level_min_meters', got "
            + std::to_string(config.levelMaxMeters) + " < "
            + std::to_string(config.levelMinMeters));
    }
    if (config.maxAdjacentDifferenceMeters <= 0)
    {
        throw std::runtime_error(
            "map_rules 'max_adjacent_difference_meters' must be > 0, got "
            + std::to_string(config.maxAdjacentDifferenceMeters));
    }
    if (config.referenceLevelMeters <= 0)
    {
        throw std::runtime_error("map_rules 'reference_level_meters' must be > 0, got "
                                 + std::to_string(config.referenceLevelMeters));
    }
    if (config.spreadAltitudeLimitMeters < config.oceanLevelMeters
        || config.spreadAltitudeLimitMeters > config.maxElevationMeters)
    {
        throw std::runtime_error(
            "map_rules 'spread_altitude_limit_meters' must be inside ["
            + std::to_string(config.oceanLevelMeters) + ", "
            + std::to_string(config.maxElevationMeters) + "], got "
            + std::to_string(config.spreadAltitudeLimitMeters));
    }
    return config;
}

} // namespace ac
