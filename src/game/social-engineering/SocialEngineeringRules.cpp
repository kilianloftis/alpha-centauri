#include "game/social-engineering/SocialEngineeringRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"

#include <variant>

namespace ac
{

namespace
{

bool PolicyForbidden_(const Faction& rFaction, const SocialPolicyId_t& rPolicyId)
{
    for (const ActiveEffect_t& rEffect : rFaction.GetActiveEffects().effects)
    {
        const auto* pForbidden =
            std::get_if<ForbiddenSocialPolicyEffect_t>(&rEffect.config->effect);
        if (pForbidden && pForbidden->policy == rPolicyId)
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy)
{
    if (!rPolicy.IsAvailable(rFaction.GetResearch().GetDiscoveredTechs()))
    {
        return false;
    }
    return !PolicyForbidden_(rFaction, rPolicy.id);
}

} // namespace ac
