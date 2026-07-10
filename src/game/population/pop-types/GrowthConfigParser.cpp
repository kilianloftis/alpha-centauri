#include "game/population/pop-types/GrowthConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace ac
{

GrowthConfig_t GrowthConfig_tParser::ParseConfig(const std::string& configPath)
{
    GrowthConfig_t config;

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open growth config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    config.nutrientsPerPop = json.value("nutrients_per_pop", config.nutrientsPerPop);
    config.maxBaseSize = json.value("max_base_size", config.maxBaseSize);

    return config;
}

} // namespace ac
