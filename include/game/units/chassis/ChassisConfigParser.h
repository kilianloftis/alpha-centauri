#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct ChassisConfig_t
{
    std::string id;
    std::string name;
    std::string requiredTech;
    int mineralCost;
    // TODO: Add chassis-specific stats (movement, hitpoints, armour slots, etc.)
};

class ChassisConfigParser
{
public:
    ChassisConfigParser() = default;
    ~ChassisConfigParser() = default;

    std::vector<ChassisConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    ChassisConfig_t ParseChassisConfig(const nlohmann::json& rChassisJson);
};

} // namespace ac
