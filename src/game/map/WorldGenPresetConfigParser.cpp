#include "game/map/WorldGenPresetConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <magic_enum.hpp>
#include <stdexcept>

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
    if (config.minElevation >= 0)
    {
        throw std::runtime_error("World gen preset '" + config.id
                                 + "' min_elevation must be < 0 (water contract)");
    }
    if (config.maxElevation < 0)
    {
        throw std::runtime_error("World gen preset '" + config.id
                                 + "' max_elevation must be >= 0");
    }

    return config;
}

WorldGenPreset_t WorldGenPresetConfigParser::ParseType_(const std::string& typeStr) const
{
    const auto parsed = magic_enum::enum_cast<WorldGenPreset_t>(typeStr);
    if (!parsed.has_value())
    {
        throw std::runtime_error("Unknown world gen preset type: '" + typeStr + "'");
    }
    return parsed.value();
}

} // namespace ac
