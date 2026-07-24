#include "game/map/WorldGenDecorationConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

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
    return config;
}

} // namespace ac
