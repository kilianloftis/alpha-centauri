#pragma once

#include "game/social-engineering/SocialPolicyConfig.h"

#include <map>

namespace ac
{

class Faction;

// Full SE selection across every category. UI draft and adopt quotes use this; it is not
// faction save state (pending/committed live on SocialEngineeringManager).
struct SocialPolicyDraft_t
{
    std::map<SocialCategory_t, const SocialPolicyConfig_t*> policies;

    const SocialPolicyConfig_t& Get(SocialCategory_t category) const;
    void Set(const SocialPolicyConfig_t& rPolicy);
};

// Snapshot of the faction's current pending selection.
SocialPolicyDraft_t PendingSocialDraft(const Faction& rFaction);

} // namespace ac
