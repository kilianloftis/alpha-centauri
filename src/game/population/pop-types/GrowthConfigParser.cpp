#include "game/population/pop-types/GrowthConfigParser.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace ac
{

GrowthConfig_t GrowthConfig_tParser::ParseConfig(const std::string& configPath)
{
    GrowthConfig_t config;

    std::ifstream file(configPath);
    if (!file.is_open())
    {
        std::cout << "Warning: Could not open '" << configPath << "', using default growth config\n";
        return config;
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    config.nutrientsPerPop = json.value("nutrients_per_pop", config.nutrientsPerPop);

    return config;
}

} // namespace ac
