#include "game/map/MapRulesConfigParser.h"

#include "lib/config/JsonConfigLoader.h"

#include <cmath>
#include <string>

namespace ac
{

namespace
{

int RequireWholeMeters_(const ConfigReader& rReader, const char* pKey)
{
    const double value = rReader.Number(pKey);
    if (value != std::floor(value))
    {
        rReader.FailKey(pKey, "must be a whole number of meters");
    }
    return static_cast<int>(value);
}

void RequireBelow_(const ConfigReader& rReader, const char* pKey, int value, const char* pBoundKey,
                   int bound)
{
    if (value >= bound)
    {
        rReader.FailKey(pKey, std::string("must be < '") + pBoundKey + "', got "
                                  + std::to_string(value) + " >= " + std::to_string(bound));
    }
}

void RequireAtLeast_(const ConfigReader& rReader, const char* pKey, int value,
                     const char* pBoundKey, int bound)
{
    if (value < bound)
    {
        rReader.FailKey(pKey, std::string("must be >= '") + pBoundKey + "', got "
                                  + std::to_string(value) + " < " + std::to_string(bound));
    }
}

void ValidateElevationRules_(const ConfigReader& rReader, const MapRulesConfig_t& rConfig)
{
    RequireBelow_(rReader, "ocean_shelf_meters", rConfig.oceanShelfMeters, "ocean_level_meters",
                  rConfig.oceanLevelMeters);
    RequireAtLeast_(rReader, "level_max_meters", rConfig.levelMaxMeters, "level_min_meters",
                    rConfig.levelMinMeters);
    RequireAtLeast_(rReader, "spread_altitude_limit_meters", rConfig.spreadAltitudeLimitMeters,
                    "ocean_level_meters", rConfig.oceanLevelMeters);
}

int RequirePositiveMeters_(const ConfigReader& rReader, const char* pKey)
{
    const int value = RequireWholeMeters_(rReader, pKey);
    if (value <= 0)
    {
        rReader.FailKey(pKey, "must be > 0, got " + std::to_string(value));
    }
    return value;
}

MapRulesConfig_t ParseMapRules_(const ConfigReader& rReader)
{
    MapRulesConfig_t config;
    config.oceanLevelMeters = RequireWholeMeters_(rReader, "ocean_level_meters");
    config.oceanShelfMeters = RequireWholeMeters_(rReader, "ocean_shelf_meters");
    config.levelMinMeters = RequirePositiveMeters_(rReader, "level_min_meters");
    config.levelMaxMeters = RequireWholeMeters_(rReader, "level_max_meters");
    config.maxAdjacentDifferenceMeters =
        RequirePositiveMeters_(rReader, "max_adjacent_difference_meters");
    config.referenceLevelMeters = RequirePositiveMeters_(rReader, "reference_level_meters");
    config.spreadAltitudeLimitMeters =
        RequireWholeMeters_(rReader, "spread_altitude_limit_meters");
    config.missingHeadquartersDistance = rReader.IntAtLeast("missing_headquarters_distance", 1);

    ValidateElevationRules_(rReader, config);
    return config;
}

} // namespace

MapRulesConfig_t MapRulesConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<MapRulesConfig_t>(configPath, "map rules",
                                                              ParseMapRules_);
}

} // namespace ac
