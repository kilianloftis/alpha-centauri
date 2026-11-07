#pragma once

#include "game/research/TechRequirement.h"
#include "game/effects/EffectConfig.h"
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
    SocialPolicyId_t id;
    std::string name;
    SocialCategory_t category;
    std::string requiredTech;  // empty if none
    // The policy every faction starts on in this category. Exactly one per category.
    bool bDefault = false;
    std::vector<EffectConfig_t> effects;

    bool IsAvailable(const std::vector<std::string>& rDiscoveredTechIds) const
    {
        return MeetsTechRequirement(requiredTech, rDiscoveredTechIds);
    }
};

} // namespace ac
