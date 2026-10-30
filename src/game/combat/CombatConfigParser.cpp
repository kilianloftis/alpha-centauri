#include "game/combat/CombatConfigParser.h"

#include "lib/config/ConfigFields.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace ac
{

CombatConfig_t CombatConfigParser::ParseConfig(const std::string& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open combat config '" + configPath + "'");
    }

    const nlohmann::json json = nlohmann::json::parse(file);
    const std::string context = "Combat config '" + configPath + "'";
    if (!json.is_object())
    {
        throw std::runtime_error(context + " must be a JSON object");
    }
    ConfigFields::RejectUnknownKeys(json, {"boarding_home_base_min_pop"}, context);

    CombatConfig_t config;
    config.boardingHomeBaseMinPop =
        ConfigFields::RequireIntAtLeast(json, "boarding_home_base_min_pop", 0, context);
    return config;
}

} // namespace ac
