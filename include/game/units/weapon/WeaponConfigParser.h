#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct WeaponConfig_t
{
    std::string id;
    std::string name;
    std::string requiredTech;
    int mineralCost;
    int attack;
    // TODO: Add weapon-specific stats (range, etc.)
};

class WeaponConfigParser
{
public:
    WeaponConfigParser() = default;
    ~WeaponConfigParser() = default;

    std::vector<WeaponConfig_t> ParseConfig(const std::string& rConfigPath);

private:
    WeaponConfig_t ParseWeaponConfig(const nlohmann::json& rWeaponJson);
};

} // namespace ac
