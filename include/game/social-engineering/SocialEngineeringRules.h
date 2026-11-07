#pragma once

namespace ac
{

class Faction;
struct SocialPolicyConfig_t;

// A policy may be adopted once its tech is known and no ForbiddenSocialPolicy blocks it.
// TODO: switching-cost / upheaval checks belong here when they land.
bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy);

} // namespace ac
