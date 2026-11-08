#pragma once

namespace ac
{

class Faction;
struct SocialPolicyConfig_t;
struct SocialPolicyDraft_t;

// A policy may be adopted once its tech is known and no ForbiddenSocialPolicy blocks it.
bool PolicyAvailable(const Faction& rFaction, const SocialPolicyConfig_t& rPolicy);

// Absolute upheaval energy if pending becomes rDraft. Compares rDraft to committed.
int QuoteUpheavalAbsolute(const Faction& rFaction, const SocialPolicyDraft_t& rDraft);

// absolute − upheavalPaid for the same draft.
int QuoteUpheavalDelta(const Faction& rFaction, const SocialPolicyDraft_t& rDraft);

// Every draft policy is PolicyAvailable, and the treasury covers a positive delta
// (refunds always pass).
bool CanAdoptDraft(const Faction& rFaction, const SocialPolicyDraft_t& rDraft);

} // namespace ac
