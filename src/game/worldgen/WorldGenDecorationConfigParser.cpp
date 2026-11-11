#include "game/worldgen/WorldGenDecorationConfigParser.h"
#include "game/worldgen/MapGenerationConfig.h"
#include "lib/config/EnumNames.h"

#include <magic_enum.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

int RequireNonNegInt_(const nlohmann::json& rJson, const char* key)
{
    if (!rJson.contains(key))
    {
        throw std::runtime_error(
            std::string("world gen decoration moisture missing required field '") + key + "'");
    }
    const int value = rJson.at(key).get<int>();
    if (value < 0)
    {
        throw std::runtime_error(
            std::string("world gen decoration moisture.") + key + " must be >= 0");
    }
    return value;
}

MoistureDecorationConfig_t ParseMoisture_(const nlohmann::json& rJson)
{
    MoistureDecorationConfig_t config;
    config.cloudmassPeaks = RequireNonNegInt_(rJson, "cloudmass_peaks");
    config.cloudmassHills = RequireNonNegInt_(rJson, "cloudmass_hills");
    config.rainfallCoeff = RequireNonNegInt_(rJson, "rainfall_coeff");
    config.hillMinElevationMeters = RequireNonNegInt_(rJson, "hill_min_elevation_meters");
    config.peakMinElevationMeters = RequireNonNegInt_(rJson, "peak_min_elevation_meters");

    if (config.cloudmassPeaks > 20)
    {
        throw std::runtime_error(
            "world gen decoration moisture.cloudmass_peaks must be <= 20");
    }
    if (config.cloudmassHills > 20)
    {
        throw std::runtime_error(
            "world gen decoration moisture.cloudmass_hills must be <= 20");
    }
    if (config.rainfallCoeff > 8)
    {
        throw std::runtime_error(
            "world gen decoration moisture.rainfall_coeff must be <= 8");
    }
    if (config.hillMinElevationMeters <= 0)
    {
        throw std::runtime_error(
            "world gen decoration moisture.hill_min_elevation_meters must be > 0");
    }
    if (config.peakMinElevationMeters <= config.hillMinElevationMeters)
    {
        throw std::runtime_error(
            "world gen decoration moisture.peak_min_elevation_meters must be > "
            "hill_min_elevation_meters");
    }

    return config;
}

RockinessWeights_t ParseRockinessWeights_(const nlohmann::json& rJson, const char* levelName)
{
    RockinessWeights_t weights;
    weights.flat = rJson.value("flat", weights.flat);
    weights.rolling = rJson.value("rolling", weights.rolling);
    weights.rocky = rJson.value("rocky", weights.rocky);

    if (weights.flat < 0.0f || weights.rolling < 0.0f || weights.rocky < 0.0f)
    {
        throw std::runtime_error(
            std::string("world gen decoration rockiness.") + levelName
            + " weights must be >= 0");
    }

    const float total = weights.flat + weights.rolling + weights.rocky;
    if (total <= 0.0f)
    {
        throw std::runtime_error(
            std::string("world gen decoration rockiness.") + levelName
            + " weights must sum to > 0");
    }

    weights.flat /= total;
    weights.rolling /= total;
    weights.rocky /= total;
    return weights;
}

RockinessWeights_t& WeightsSlot_(RockinessDecorationConfig_t& rConfig, ErosiveForces_t level)
{
    switch (level)
    {
    case ErosiveForces_t::Low:
        return rConfig.low;
    case ErosiveForces_t::Average:
        return rConfig.average;
    case ErosiveForces_t::High:
        return rConfig.high;
    }
    throw std::runtime_error("Unknown erosive forces level");
}

RockinessDecorationConfig_t ParseRockiness_(const nlohmann::json& rJson)
{
    RockinessDecorationConfig_t config;
    for (const ErosiveForces_t level : magic_enum::enum_values<ErosiveForces_t>())
    {
        const std::string key = EnumToLowerName(level);
        if (!rJson.contains(key))
        {
            throw std::runtime_error(
                "world gen decoration rockiness missing required level '" + key + "'");
        }
        WeightsSlot_(config, level) =
            ParseRockinessWeights_(rJson.at(key), key.c_str());
    }
    return config;
}

AquiferDecorationConfig_t ParseAquifers_(const nlohmann::json& rJson)
{
    AquiferDecorationConfig_t config;
    if (!rJson.contains("fraction"))
    {
        throw std::runtime_error(
            "world gen decoration aquifers missing required field 'fraction'");
    }
    config.fraction = rJson.at("fraction").get<float>();
    if (config.fraction < 0.0f || config.fraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration aquifers.fraction must be in [0, 1]");
    }
    return config;
}

FungusDecorationConfig_t ParseFungus_(const nlohmann::json& rJson)
{
    FungusDecorationConfig_t config;
    if (!rJson.contains("band_starts") || !rJson.at("band_starts").is_array()
        || rJson.at("band_starts").empty())
    {
        throw std::runtime_error(
            "world gen decoration fungus.band_starts must be a non-empty array");
    }
    if (!rJson.contains("band_width"))
    {
        throw std::runtime_error(
            "world gen decoration fungus missing required field 'band_width'");
    }
    if (!rJson.contains("pole_row_chance"))
    {
        throw std::runtime_error(
            "world gen decoration fungus missing required field 'pole_row_chance'");
    }

    config.bandStarts.clear();
    for (const nlohmann::json& rStart : rJson.at("band_starts"))
    {
        if (!rStart.is_number_integer())
        {
            throw std::runtime_error(
                "world gen decoration fungus.band_starts entries must be integers");
        }
        const int start = rStart.get<int>();
        if (start < 0 || start > 100)
        {
            throw std::runtime_error(
                "world gen decoration fungus.band_starts entries must be in [0, 100]");
        }
        config.bandStarts.push_back(start);
    }
    config.bandWidth = rJson.at("band_width").get<int>();
    if (config.bandWidth < 0)
    {
        throw std::runtime_error(
            "world gen decoration fungus.band_width must be >= 0");
    }
    config.poleRowChance = rJson.at("pole_row_chance").get<float>();
    if (config.poleRowChance < 0.0f || config.poleRowChance > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration fungus.pole_row_chance must be in [0, 1]");
    }
    return config;
}

TileBonusDecorationConfig_t ParseTileBonuses_(const nlohmann::json& rJson)
{
    TileBonusDecorationConfig_t config;
    if (!rJson.contains("fraction"))
    {
        throw std::runtime_error(
            "world gen decoration tile_bonuses missing required field 'fraction'");
    }
    config.fraction = rJson.at("fraction").get<float>();
    if (config.fraction < 0.0f || config.fraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration tile_bonuses.fraction must be in [0, 1]");
    }
    return config;
}

const nlohmann::json& RequireSection_(const nlohmann::json& rJson, const char* pKey)
{
    if (!rJson.contains(pKey) || !rJson.at(pKey).is_object())
    {
        throw std::runtime_error(
            std::string("world gen decoration config missing required object '") + pKey + "'");
    }
    return rJson.at(pKey);
}

} // namespace

WorldGenDecorationConfig_t WorldGenDecorationConfigParser::ParseConfig(
    const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open world gen decoration config '"
                                 + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    WorldGenDecorationConfig_t config;
    config.moisture = ParseMoisture_(RequireSection_(json, "moisture"));
    config.rockiness = ParseRockiness_(RequireSection_(json, "rockiness"));
    config.aquifers = ParseAquifers_(RequireSection_(json, "aquifers"));
    config.fungus = ParseFungus_(RequireSection_(json, "fungus"));
    config.tileBonuses = ParseTileBonuses_(RequireSection_(json, "tile_bonuses"));
    return config;
}

} // namespace ac
