#include "game/map/WorldGenPresetConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/EnumNames.h"
#include "lib/config/JsonConfigLoader.h"

#include <stdexcept>
#include <string>

namespace ac
{

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

    config.minElevation = presetJson.value("min_elevation", config.minElevation);
    config.maxElevation = presetJson.value("max_elevation", config.maxElevation);

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
    if (rPreset.minElevation < rMapRules.minElevationMeters
        || rPreset.maxElevation > rMapRules.maxElevationMeters)
    {
        throw std::runtime_error(
            "World gen preset '" + rPreset.id + "' elevation range ["
            + std::to_string(rPreset.minElevation) + ", "
            + std::to_string(rPreset.maxElevation) + "] is outside map_rules ["
            + std::to_string(rMapRules.minElevationMeters) + ", "
            + std::to_string(rMapRules.maxElevationMeters) + "]");
    }
}

WorldGenPreset_t WorldGenPresetConfigParser::ParseType_(const std::string& typeStr) const
{
    // Case-insensitive, matching every other enum-valued config field.
    return EnumFromName<WorldGenPreset_t>(typeStr, "world gen preset type");
}

} // namespace ac
