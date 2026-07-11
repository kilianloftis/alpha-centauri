#pragma once

#include "game/GameCategory.h"
#include "game/IConstructable.h"
#include "game/effects/BonusEffect.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

struct BuildingConfig_t : public IConstructable
{
    std::string id;
    std::string name;
    GameCategory_t category;
    int mineralCost;
    std::string requiredTech;  // empty if none — same convention as SocialPolicyConfig_t, etc.
    bool allowMultiple;
    bool bIsSecretProject;
    std::vector<EffectConfig_t> effects;

    const std::string& GetId() const override { return id; }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return mineralCost; }

    // Empty requiredTech = always available (matches SocialPolicyConfig_t::IsAvailable).
    bool IsAvailable(const std::vector<std::string>& discoveredTechs) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(discoveredTechs.begin(), discoveredTechs.end(), requiredTech)
               != discoveredTechs.end();
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
