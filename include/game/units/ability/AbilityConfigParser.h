#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct AbilityConfig_t
{
    std::string id;
    std::string name;
    std::string requiredTech;
    int mineralCost;
    // TODO: Add ability-specific behaviour (effect type, parameters, etc.)
};

class AbilityConfigParser
{
public:
    AbilityConfigParser() = default;
    ~AbilityConfigParser() = default;

    std::vector<AbilityConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    AbilityConfig_t ParseAbilityConfig(const nlohmann::json& rAbilityJson);
};

} // namespace ac
