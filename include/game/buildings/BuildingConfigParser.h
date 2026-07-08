#pragma once

#include "game/GameCategory.h"
#include "game/IConstructable.h"
#include "lib/effects/BonusEffect.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct BuildingConfig_t : public IConstructable
{
    std::string id;
    std::string name;
    GameCategory category;
    int mineralCost;
    std::vector<std::string> requiredTechs;
    bool allowMultiple;
    bool bIsSecretProject;
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
    BuildingConfig_t ParseBuildingConfig(const nlohmann::json& buildingJson);
};

} // namespace ac
