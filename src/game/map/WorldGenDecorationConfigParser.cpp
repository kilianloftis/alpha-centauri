#include "game/map/WorldGenDecorationConfigParser.h"
#include "game/map/MapGenerationConfig.h"
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

MoistureDecorationConfig_t ParseMoisture_(const nlohmann::json& rJson)
{
    MoistureDecorationConfig_t config;
    config.baseMin = rJson.value("base_min", config.baseMin);
    config.baseRange = rJson.value("base_range", config.baseRange);
    config.coastalPeakBonus = rJson.value("coastal_peak_bonus", config.coastalPeakBonus);
    config.coastalRadius = rJson.value("coastal_radius", config.coastalRadius);
    config.tropicalPeakBonus = rJson.value("tropical_peak_bonus", config.tropicalPeakBonus);
    config.tropicalHalfWidth = rJson.value("tropical_half_width", config.tropicalHalfWidth);
    config.orographicStrength = rJson.value("orographic_strength", config.orographicStrength);
    config.orographicElevScale = rJson.value("orographic_elev_scale", config.orographicElevScale);
    config.orographicMaxElev = rJson.value("orographic_max_elev", config.orographicMaxElev);
    config.aridThreshold = rJson.value("arid_threshold", config.aridThreshold);
    config.moistThreshold = rJson.value("moist_threshold", config.moistThreshold);

    if (config.coastalRadius < 1)
    {
        throw std::runtime_error("world gen decoration moisture.coastal_radius must be >= 1");
    }
    if (config.baseRange < 0.0f)
    {
        throw std::runtime_error("world gen decoration moisture.base_range must be >= 0");
    }
    if (config.tropicalHalfWidth <= 0.0f || config.tropicalHalfWidth > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration moisture.tropical_half_width must be in (0, 1]");
    }
    if (config.orographicElevScale <= 0.0f || config.orographicMaxElev <= 0.0f)
    {
        throw std::runtime_error(
            "world gen decoration moisture orographic elevation scales must be > 0");
    }
    if (config.aridThreshold >= config.moistThreshold)
    {
        throw std::runtime_error(
            "world gen decoration moisture.arid_threshold must be < moist_threshold");
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
    if (!rJson.contains("land_fraction"))
    {
        throw std::runtime_error(
            "world gen decoration aquifers missing required field 'land_fraction'");
    }
    config.landFraction = rJson.at("land_fraction").get<float>();
    if (config.landFraction < 0.0f || config.landFraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration aquifers.land_fraction must be in [0, 1]");
    }
    return config;
}

FungusDecorationConfig_t ParseFungus_(const nlohmann::json& rJson)
{
    FungusDecorationConfig_t config;
    if (!rJson.contains("land_fraction"))
    {
        throw std::runtime_error(
            "world gen decoration fungus missing required field 'land_fraction'");
    }
    config.landFraction = rJson.at("land_fraction").get<float>();
    config.waterFraction = rJson.value("water_fraction", config.waterFraction);
    config.minPatchTiles = rJson.value("min_patch_tiles", config.minPatchTiles);
    config.maxPatchTiles = rJson.value("max_patch_tiles", config.maxPatchTiles);
    config.patchSizeSkew = rJson.value("patch_size_skew", config.patchSizeSkew);

    if (config.landFraction < 0.0f || config.landFraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration fungus.land_fraction must be in [0, 1]");
    }
    if (config.waterFraction < 0.0f || config.waterFraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration fungus.water_fraction must be in [0, 1]");
    }
    if (config.minPatchTiles < 1)
    {
        throw std::runtime_error(
            "world gen decoration fungus.min_patch_tiles must be >= 1");
    }
    if (config.maxPatchTiles < config.minPatchTiles)
    {
        throw std::runtime_error(
            "world gen decoration fungus.max_patch_tiles must be >= min_patch_tiles");
    }
    if (config.patchSizeSkew < 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration fungus.patch_size_skew must be >= 1");
    }
    return config;
}

TileBonusDecorationConfig_t ParseTileBonuses_(const nlohmann::json& rJson)
{
    TileBonusDecorationConfig_t config;
    if (!rJson.contains("land_fraction"))
    {
        throw std::runtime_error(
            "world gen decoration tile_bonuses missing required field 'land_fraction'");
    }
    config.landFraction = rJson.at("land_fraction").get<float>();
    if (config.landFraction < 0.0f || config.landFraction > 1.0f)
    {
        throw std::runtime_error(
            "world gen decoration tile_bonuses.land_fraction must be in [0, 1]");
    }
    return config;
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
    if (json.contains("moisture"))
    {
        config.moisture = ParseMoisture_(json.at("moisture"));
    }
    if (!json.contains("rockiness") || !json.at("rockiness").is_object())
    {
        throw std::runtime_error(
            "world gen decoration config missing required object 'rockiness'");
    }
    config.rockiness = ParseRockiness_(json.at("rockiness"));
    if (!json.contains("aquifers") || !json.at("aquifers").is_object())
    {
        throw std::runtime_error(
            "world gen decoration config missing required object 'aquifers'");
    }
    config.aquifers = ParseAquifers_(json.at("aquifers"));
    if (!json.contains("fungus") || !json.at("fungus").is_object())
    {
        throw std::runtime_error(
            "world gen decoration config missing required object 'fungus'");
    }
    config.fungus = ParseFungus_(json.at("fungus"));
    if (!json.contains("tile_bonuses") || !json.at("tile_bonuses").is_object())
    {
        throw std::runtime_error(
            "world gen decoration config missing required object 'tile_bonuses'");
    }
    config.tileBonuses = ParseTileBonuses_(json.at("tile_bonuses"));
    return config;
}

} // namespace ac
