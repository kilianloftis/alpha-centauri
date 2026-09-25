#include "game/map/WorldGenPresetConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

int RequirePresetMeters_(const nlohmann::json& rJson, const char* pKey, const std::string& rPresetId)
{
    if (!rJson.contains(pKey) || !rJson.at(pKey).is_number_integer())
    {
        throw std::runtime_error("World gen preset '" + rPresetId + "' requires integer '"
                                 + pKey + "'");
    }
    return rJson.at(pKey).get<int>();
}

} // namespace

std::vector<WorldGenPresetConfig_t> WorldGenPresetConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<WorldGenPresetConfig_t>(
        configPath, "world gen preset",
        [this](const nlohmann::json& rJson) { return ParsePresetConfig_(rJson); });
}

WorldGenPresetConfig_t WorldGenPresetConfigParser::ParsePresetConfig_(
    const nlohmann::json& presetJson)
{
    WorldGenPresetConfig_t config;
    config.id = ConfigFields::ParseId(presetJson);
    config.name = ConfigFields::ParseName(presetJson, config.id);
    config.type = ParseType_(presetJson.at("type").get<std::string>());

    config.frequency = presetJson.value("frequency", config.frequency);
    config.octaves = presetJson.value("octaves", config.octaves);
    config.lacunarity = presetJson.value("lacunarity", config.lacunarity);
    config.gain = presetJson.value("gain", config.gain);

    config.continentScale = presetJson.value("continent_scale", config.continentScale);
    config.centerBias = presetJson.value("center_bias", config.centerBias);
    config.edgeFalloff = presetJson.value("edge_falloff", config.edgeFalloff);

    config.minElevation = RequirePresetMeters_(presetJson, "min_elevation", config.id);
    config.maxElevation = RequirePresetMeters_(presetJson, "max_elevation", config.id);
    if (config.minElevation >= config.maxElevation)
    {
        throw std::runtime_error("World gen preset '" + config.id
                                 + "' min_elevation must be < max_elevation, got "
                                 + std::to_string(config.minElevation) + " >= "
                                 + std::to_string(config.maxElevation));
    }

    if (config.octaves < 1)
    {
        throw std::runtime_error("World gen preset '" + config.id
                                 + "' has invalid octaves (must be >= 1)");
    }
    return config;
}

void WorldGenPresetConfigParser::ValidateAgainstMapRules(
    const WorldGenPresetConfig_t& rPreset, const ElevationRulesConfig_t& rMapRules)
{
    if (rPreset.minElevation >= rPreset.maxElevation)
    {
        throw std::runtime_error("World gen preset '" + rPreset.id
                                 + "' min_elevation must be < max_elevation, got "
                                 + std::to_string(rPreset.minElevation) + " >= "
                                 + std::to_string(rPreset.maxElevation));
    }
    if (rPreset.minElevation >= rMapRules.oceanLevelMeters)
    {
        throw std::runtime_error("World gen preset '" + rPreset.id
                                 + "' min_elevation must be < map ocean_level_meters ("
                                 + std::to_string(rMapRules.oceanLevelMeters) + ")");
    }
    if (rPreset.maxElevation < rMapRules.oceanLevelMeters)
    {
        throw std::runtime_error("World gen preset '" + rPreset.id
                                 + "' max_elevation must be >= map ocean_level_meters ("
                                 + std::to_string(rMapRules.oceanLevelMeters) + ")");
    }
    if (rMapRules.oceanShelfMeters < rPreset.minElevation)
    {
        throw std::runtime_error("World gen preset '" + rPreset.id
                                 + "' min_elevation must be <= map ocean_shelf_meters ("
                                 + std::to_string(rMapRules.oceanShelfMeters) + ")");
    }
    if (rPreset.maxElevation < rMapRules.spreadAltitudeLimitMeters)
    {
        throw std::runtime_error(
            "World gen preset '" + rPreset.id
            + "' max_elevation must be >= map spread_altitude_limit_meters ("
            + std::to_string(rMapRules.spreadAltitudeLimitMeters) + ")");
    }
}

void WorldGenPresetConfigParser::ApplyElevationRange(ElevationRulesConfig_t& rRules,
                                                     const WorldGenPresetConfig_t& rPreset)
{
    ValidateAgainstMapRules(rPreset, rRules);
    rRules.minElevationMeters = rPreset.minElevation;
    rRules.maxElevationMeters = rPreset.maxElevation;
}

WorldGenPreset_t WorldGenPresetConfigParser::ParseType_(const std::string& typeStr) const
{
    // Case-insensitive, matching every other enum-valued config field.
    return EnumFromName<WorldGenPreset_t>(typeStr, "world gen preset type");
}

} // namespace ac
