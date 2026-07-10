#pragma once

#include "game/effects/BonusEffect.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

enum class SocialCategory_t
{
    Politics,
    Economics,
    Values,
    FutureSociety
};

struct SocialPolicyConfig_t
{
    std::string id;
    std::string name;
    SocialCategory_t category;
    std::string requiredTech;  // empty if none
    std::vector<EffectConfig_t> effects;

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechIds) const
    {
        if (requiredTech.empty())
        {
            return true;
        }
        return std::find(rDiscoveredTechIds.begin(), rDiscoveredTechIds.end(), requiredTech)
               != rDiscoveredTechIds.end();
    }
};

} // namespace ac
