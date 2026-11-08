#pragma once

namespace ac
{

class Faction;
struct SocialPolicyDraft_t;

// Pay or refund the upheaval delta for adopting rDraft as pending, then set pending.
void AdoptDraft(Faction& rFaction, const SocialPolicyDraft_t& rDraft);

// Copy pending into committed and clear upheavalPaid (end of turn).
void CommitSocialEngineering(Faction& rFaction);

} // namespace ac
