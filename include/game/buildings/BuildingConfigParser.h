#pragma once

#include "game/GameCategory.h"
#include "game/IConstructable.h"
#include "game/effects/EffectConfig.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

using BuildingId_t = std::string;

struct BuildingConfig_t : public IConstructable
{
    BuildingId_t id;
    std::string name;
    GameCategory_t category;
    int mineralCost;
    std::string requiredTech;  // empty if none — same convention as SocialPolicyConfig_t, etc.
    bool allowMultiple;
    bool bIsSecretProject;
    // Public orbital census: counts of buildings with this flag are visible to all factions.
    bool orbital = false;
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
