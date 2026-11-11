#include "game/worldgen/WorldGenDecorationConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
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

RockinessWeights_t ParseRockinessWeights_(const nlohmann::json& rJson, const char* pContext)
{
    if (!rJson.is_object() || !rJson.contains("flat") || !rJson.contains("rolling")
        || !rJson.contains("rocky"))
    {
        throw std::runtime_error(
            std::string("world gen decoration ") + pContext
            + " must be an object with flat, rolling, and rocky");
    }

    RockinessWeights_t weights;
    weights.flat = rJson.at("flat").get<float>();
    weights.rolling = rJson.at("rolling").get<float>();
    weights.rocky = rJson.at("rocky").get<float>();

    if (weights.flat < 0.0f || weights.rolling < 0.0f || weights.rocky < 0.0f)
    {
        throw std::runtime_error(
            std::string("world gen decoration ") + pContext + " weights must be >= 0");
    }

    const float total = weights.flat + weights.rolling + weights.rocky;
    if (total <= 0.0f)
    {
        throw std::runtime_error(
            std::string("world gen decoration ") + pContext + " weights must sum to > 0");
    }

    weights.flat /= total;
    weights.rolling /= total;
    weights.rocky /= total;
    return weights;
}

RockinessDecorationConfig_t ParseRockiness_(const nlohmann::json& rJson)
{
    if (!rJson.contains("poles") || !rJson.contains("bands") || !rJson.at("bands").is_array()
        || rJson.at("bands").empty())
    {
        throw std::runtime_error(
            "world gen decoration rockiness requires poles and a non-empty bands array");
    }

    RockinessDecorationConfig_t config;
    config.poles = ParseRockinessWeights_(rJson.at("poles"), "rockiness.poles");
    config.bands.clear();

    const nlohmann::json& rBands = rJson.at("bands");
    const size_t bandCount = rBands.size();
    std::optional<int> previousMax;
    for (size_t i = 0; i < bandCount; ++i)
    {
        const nlohmann::json& rBand = rBands.at(i);
        if (!rBand.is_object())
        {
            throw std::runtime_error("world gen decoration rockiness.bands entries must be objects");
        }

        const bool bLast = (i + 1 == bandCount);
        const bool bHasMax = rBand.contains("max_elevation_above_ocean_meters");
        if (bLast && bHasMax)
        {
            throw std::runtime_error(
                "world gen decoration rockiness.bands last entry must omit "
                "max_elevation_above_ocean_meters");
        }
        if (!bLast && !bHasMax)
        {
            throw std::runtime_error(
                "world gen decoration rockiness.bands non-final entries require "
                "max_elevation_above_ocean_meters");
        }

        RockinessBand_t band;
        if (bHasMax)
        {
            const int maxElevation = rBand.at("max_elevation_above_ocean_meters").get<int>();
            if (previousMax.has_value() && maxElevation <= *previousMax)
            {
                throw std::runtime_error(
                    "world gen decoration rockiness.bands max_elevation_above_ocean_meters "
                    "must be strictly increasing");
            }
            band.maxElevationAboveOceanMeters = maxElevation;
            previousMax = maxElevation;
        }

        const std::string weightContext = "rockiness.bands[" + std::to_string(i) + "]";
        band.weights = ParseRockinessWeights_(rBand, weightContext.c_str());
        config.bands.push_back(band);
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

float RequireUnitInterval_(const nlohmann::json& rJson, const char* pKey, const char* pContext)
{
    if (!rJson.contains(pKey))
    {
        throw std::runtime_error(
            std::string("world gen decoration ") + pContext + " missing required field '" + pKey
            + "'");
    }
    const float value = rJson.at(pKey).get<float>();
    if (value < 0.0f || value > 1.0f)
    {
        throw std::runtime_error(
            std::string("world gen decoration ") + pContext + "." + pKey + " must be in [0, 1]");
    }
    return value;
}

FungusDecorationConfig_t ParseFungus_(const nlohmann::json& rJson)
{
    FungusDecorationConfig_t config;
    config.coverage = RequireUnitInterval_(rJson, "coverage", "fungus");
    config.fill = RequireUnitInterval_(rJson, "fill", "fungus");
    config.poleRowChance = RequireUnitInterval_(rJson, "pole_row_chance", "fungus");
    if (!rJson.contains("cluster_scale_tiles"))
    {
        throw std::runtime_error(
            "world gen decoration fungus missing required field 'cluster_scale_tiles'");
    }
    config.clusterScaleTiles = rJson.at("cluster_scale_tiles").get<int>();
    if (config.clusterScaleTiles < 1)
    {
        throw std::runtime_error(
            "world gen decoration fungus.cluster_scale_tiles must be >= 1");
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
