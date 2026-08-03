#include "game/units/BaseConquestConfigParser.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

BaseConquestConfig_t BaseConquestConfigParser::ParseConfig(const std::string& configPath) const
{
    std::ifstream file(configPath);
    if (!file)
    {
        throw std::runtime_error("BaseConquestConfigParser: cannot open " + configPath);
    }

    nlohmann::json json;
    file >> json;

    BaseConquestConfig_t config;
    config.lastDefenderPopLoss =
        json.value("last_defender_pop_loss", config.lastDefenderPopLoss);
    config.capturePopLoss = json.value("capture_pop_loss", config.capturePopLoss);
    config.captureFacilitiesDestroyedMin =
        json.value("capture_facilities_destroyed_min", config.captureFacilitiesDestroyedMin);
    config.captureFacilitiesDestroyedMaxPercent = json.value(
        "capture_facilities_destroyed_max_percent", config.captureFacilitiesDestroyedMaxPercent);

    if (json.contains("escape_colony_pod"))
    {
        const nlohmann::json& rPod = json.at("escape_colony_pod");
        if (rPod.contains("components"))
        {
            config.escapeColonyPod.componentIds =
                rPod.at("components").get<std::vector<std::string>>();
        }
    }

    if (config.lastDefenderPopLoss < 0 || config.capturePopLoss < 0)
    {
        throw std::runtime_error("base_conquest.json: pop loss values must be >= 0");
    }
    if (config.captureFacilitiesDestroyedMin < 0
        || config.captureFacilitiesDestroyedMaxPercent < 0
        || config.captureFacilitiesDestroyedMaxPercent > 100)
    {
        throw std::runtime_error(
            "base_conquest.json: facility destruction range is invalid");
    }
    return config;
}

} // namespace ac
