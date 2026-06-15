#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ac
{

struct BuildingImprovementBonus_t
{
    int nutrients;
};

struct BuildingConfig_t
{
    std::string id;
    std::string name;
    int mineralCost;
    std::vector<std::string> requiredTechs;
    int nutrientsBonus;
    bool allowMultiple;
    std::unordered_map<std::string, BuildingImprovementBonus_t> improvementBonuses;

    bool IsDiscovered(const std::vector<std::string>& discoveredTechs) const
    {
        for (const auto& tech : requiredTechs)
        {
            if (std::find(discoveredTechs.begin(), discoveredTechs.end(), tech) != discoveredTechs.end())
            {
                return true;
            }
        }
        return false;
    }
};

class BuildingConfigParser
{
public:
    BuildingConfigParser();
    ~BuildingConfigParser() = default;

    std::vector<BuildingConfig_t> ParseConfig(const std::string& configPath);

private:
    BuildingConfig_t ParseBuildingConfig(const nlohmann::json& buildingJson);
};

} // namespace ac
