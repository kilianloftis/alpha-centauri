#pragma once

#include "game/IConstructable.h"
#include "lib/effects/BonusEffect.h"
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

struct BuildingConfig_t : public IConstructable
{
    std::string id;
    std::string name;
    int mineralCost;
    std::vector<std::string> requiredTechs;
    int nutrientsBonus;
    bool allowMultiple;
    bool bIsSecretProject;
    std::unordered_map<std::string, BuildingImprovementBonus_t> improvementBonuses;
    std::vector<EffectConfig_t> effects;

    const char* GetId() const override { return id.c_str(); }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return mineralCost; }

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
    std::vector<BuildingConfig_t> ParseFile_(const std::string& filePath);
    BuildingConfig_t ParseBuildingConfig(const nlohmann::json& buildingJson);
    EffectConfig_t ParseBuildingEffect(const nlohmann::json& effectJson);
};

} // namespace ac
