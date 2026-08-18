#pragma once

#include "game/GameCategory.h"
#include "game/IConstructable.h"
#include "game/effects/EffectConfig.h"
#include "game/faction/base/production/ScrapConfig.h"
#include <algorithm>
#include <string>
#include <vector>

// Data only, separate from BuildingConfigParser.h so consumers do not pull <nlohmann/json.hpp>.

namespace ac
{

using BuildingId_t = std::string;

struct BuildingConfig_t : public IConstructable
{
    BuildingId_t id;
    std::string name;
    GameCategory_t category = GameCategory_t::Build;
    int mineralCost = 0;
    // Per-turn energy-credit maintenance; summed across owned copies and charged in Upkeep.
    int upkeep = 0;
    std::string requiredTech;  // empty if none — same convention as SocialPolicyConfig_t, etc.
    bool allowMultiple = false;
    bool bIsSecretProject = false;
    // Public orbital census: counts of buildings with this flag are visible to all factions.
    bool orbital = false;
    std::vector<EffectConfig_t> effects;
    // Optional partial override of kinds.building.default_scrap. `"formula": null` denies
    // scrap. Secret projects reject this.
    std::optional<ScrapOverride_t> scrap;

    const std::string& GetId() const override { return id; }
    const std::string& GetName() const override { return name; }
    int GetBaseCost() const override { return mineralCost; }
    ConstructableKind_t GetConstructableKind() const override
    {
        return bIsSecretProject ? ConstructableKind_t::SecretProject : ConstructableKind_t::Building;
    }
    // Per-turn energy-credit maintenance for this building type (UI / upkeep stage).
    int GetUpkeep() const { return upkeep; }

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

} // namespace ac
