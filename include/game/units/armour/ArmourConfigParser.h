#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct ArmourConfig_t
{
    std::string id;
    std::string name;
    std::string requiredTech;
    int mineralCost;
    int defense;
    // TODO: Add armour-specific stats (etc.)
};

class ArmourConfigParser
{
public:
    ArmourConfigParser() = default;
    ~ArmourConfigParser() = default;

    std::vector<ArmourConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    ArmourConfig_t ParseArmourConfig(const nlohmann::json& rArmourJson);
};

} // namespace ac
