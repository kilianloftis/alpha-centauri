#pragma once

#include "lib/effects/BonusEffect.h"
#include <algorithm>
#include <string>
#include <vector>

namespace ac
{

enum class SocialCategory
{
    Politics,
    Economics,
    Values,
    FutureSociety
};

struct SocialPolicyConfig
{
    std::string id;
    std::string name;
    SocialCategory category;
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
