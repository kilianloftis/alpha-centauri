#pragma once

#include "game/GameCategory.h"
#include "game/IConstructable.h"
#include "game/effects/EffectConfig.h"
#include <algorithm>
#include <string>
#include <vector>

// Data only. Deliberately separate from BuildingConfigParser.h so that including a building
// config does not drag in <nlohmann/json.hpp> — this type is reached from BaseManager,
// BuildingManager, the orbital census, probe effects and the UI, none of which parse JSON.

namespace ac
{

using BuildingId_t = std::string;

struct BuildingConfig_t : public IConstructable
{
    BuildingId_t id;
    std::string name;
    // Every member has a default: a parser that forgets a field, or a struct built by hand in a
    // test, must not start from indeterminate values.
    GameCategory_t category = GameCategory_t::Build;
    int mineralCost = 0;
    std::string requiredTech;  // empty if none — same convention as SocialPolicyConfig_t, etc.
    bool allowMultiple = false;
    bool bIsSecretProject = false;
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

} // namespace ac
