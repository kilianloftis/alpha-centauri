#include "game/social-engineering/SocialPolicyDraft.h"

#include "game/faction/Faction.h"
#include "game/social-engineering/SocialEngineeringManager.h"

#include <magic_enum.hpp>
#include <stdexcept>

namespace ac
{

const SocialPolicyConfig_t& SocialPolicyDraft_t::Get(SocialCategory_t category) const
{
    const auto it = policies.find(category);
    if (it == policies.end() || !it->second)
    {
        throw std::runtime_error("SocialPolicyDraft_t::Get: missing policy for category");
    }
    return *it->second;
}

void SocialPolicyDraft_t::Set(const SocialPolicyConfig_t& rPolicy)
{
    policies[rPolicy.category] = &rPolicy;
}

SocialPolicyDraft_t PendingSocialDraft(const Faction& rFaction)
{
    SocialPolicyDraft_t draft;
    const SocialEngineeringManager& rSe = rFaction.GetSocialEngineering();
    for (const SocialCategory_t category : magic_enum::enum_values<SocialCategory_t>())
    {
        draft.policies[category] = &rSe.GetActivePolicy(category);
    }
    return draft;
}

} // namespace ac
