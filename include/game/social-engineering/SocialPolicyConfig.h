#pragma once

#include "game/social-engineering/SocialEffects.h"
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
    std::string prerequisiteTech;  // empty if none
    SocialScores effects;

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechIds) const
    {
        if (prerequisiteTech.empty())
        {
            return true;
        }
        return std::find(rDiscoveredTechIds.begin(), rDiscoveredTechIds.end(), prerequisiteTech)
               != rDiscoveredTechIds.end();
    }
};

} // namespace ac
