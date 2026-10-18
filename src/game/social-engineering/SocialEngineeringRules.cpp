#include "game/social-engineering/SocialEngineeringRules.h"

#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"

namespace ac
{

bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy)
{
    return rPolicy.IsAvailable(rFaction.GetResearch().GetDiscoveredTechs());
}

} // namespace ac
