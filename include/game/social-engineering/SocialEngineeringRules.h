#pragma once

namespace ac
{

class Faction;
struct SocialPolicyConfig_t;

// A policy may be adopted once its tech is known.
// TODO: SetActivePolicy has no legality or switching-cost check; when one lands it goes here.
bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy);

} // namespace ac
