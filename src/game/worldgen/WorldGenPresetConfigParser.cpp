#include "game/worldgen/WorldGenPresetConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

void ParseNoise_(const ConfigReader& rReader, WorldGenPresetConfig_t& rConfig)
{
    rConfig.frequency = rReader.FloatOr("frequency", rConfig.frequency);
    if (rReader.Has("octaves"))
    {
        rConfig.octaves = rReader.IntAtLeast("octaves", 1);
    }
    rConfig.lacunarity = rReader.FloatOr("lacunarity", rConfig.lacunarity);
    rConfig.gain = rReader.FloatOr("gain", rConfig.gain);
}

void ParseLandmass_(const ConfigReader& rReader, WorldGenPresetConfig_t& rConfig)
{
    rConfig.continentScale = rReader.FloatOr("continent_scale", rConfig.continentScale);
    rConfig.centerBias = rReader.FloatOr("center_bias", rConfig.centerBias);
    rConfig.edgeFalloff = rReader.FloatOr("edge_falloff", rConfig.edgeFalloff);
    if (rReader.Has("ocean_depth_exponent"))
    {
        rConfig.oceanDepthExponent =
            static_cast<float>(rReader.NumberAbove("ocean_depth_exponent", 0.0));
    }
}

WorldGenPresetConfig_t ParsePreset_(const ConfigReader& rReader)
{
    WorldGenPresetConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    // Case-insensitive, matching every other enum-valued config field.
    config.type = rReader.Enum<WorldGenPreset_t>("type", "world gen preset type");
    ParseNoise_(rReader, config);
    ParseLandmass_(rReader, config);
    config.minElevation = rReader.Int("min_elevation");
    config.maxElevation = rReader.Int("max_elevation");
    if (config.minElevation >= config.maxElevation)
    {
        rReader.FailKey("min_elevation", "must be < max_elevation, got "
                                             + std::to_string(config.minElevation) + " >= "
                                             + std::to_string(config.maxElevation));
    }
    return config;
}

} // namespace

std::vector<WorldGenPresetConfig_t> WorldGenPresetConfigParser::ParseConfig(
    const std::string& configPath)
{
    return JsonConfigLoader::LoadFile<WorldGenPresetConfig_t>(configPath, "world gen preset",
                                                              ParsePreset_);
}

void WorldGenPresetConfigParser::ValidateAgainstMapRules(
    const WorldGenPresetConfig_t& rPreset, const MapRulesConfig_t& rMapRules)
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

void WorldGenPresetConfigParser::ApplyElevationRange(MapRulesConfig_t& rRules,
                                                     const WorldGenPresetConfig_t& rPreset)
{
    ValidateAgainstMapRules(rPreset, rRules);
    rRules.minElevationMeters = rPreset.minElevation;
    rRules.maxElevationMeters = rPreset.maxElevation;
}

} // namespace ac
