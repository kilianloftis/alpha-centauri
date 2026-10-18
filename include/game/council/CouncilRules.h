#pragma once

#include "game/council/CouncilProposalConfig.h"
#include "game/council/PendingProposal.h"

#include <string>
#include <vector>

namespace ac
{

class Faction;
class PlanetaryCouncil;

bool HasCommlinksToAllMembers(const PlanetaryCouncil& rCouncil, const Faction& rFaction);

// The governor may propose more often than an ordinary member.
int ProposeCooldownYears(const PlanetaryCouncil& rCouncil, const Faction& rFaction);
int YearsUntilCanPropose(const PlanetaryCouncil& rCouncil, const Faction& rFaction);

// Proposal eligibility (membership, tech, active and repeal gates, consumption). Commlinks and
// cooldown are the propose-time checks it leaves out.
bool CanPropose(const PlanetaryCouncil& rCouncil, const Faction& rProposer,
                const std::string& rProposalId);

// A member's weight under the given mode, with CouncilVotes effects applied.
int VoteWeight(const PlanetaryCouncil& rCouncil, const Faction& rFaction,
               CouncilVoteWeight_t mode);

// The two most populous members, ties to the lower faction id.
std::vector<Faction*> GovernorCandidates(const PlanetaryCouncil& rCouncil);

// GovernorCandidates for a Planetary Governor election, every member for Supreme Leader.
std::vector<Faction*> EligibleCandidates(const PlanetaryCouncil& rCouncil,
                                         const CouncilProposalConfig_t& rConfig);

// Weighted Yea outvotes Nay, or with a vote threshold reaches that share of the total member
// weight, so an abstention (or a missing ballot) counts against the bar.
bool ProposalPasses(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending);

// The eligible member with the most weighted votes, ties to the lower faction id, when it clears
// the threshold; null when the election fails.
Faction* ElectionWinner(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending);

// The governor may veto a standard ballot; the office cannot be defended in an election.
bool CanVeto(const PlanetaryCouncil& rCouncil, const Faction& rFaction,
             const PendingProposal_t& rPending);

// A veto stands unless every member other than the governor voted Yea.
bool VetoOverruled(const PlanetaryCouncil& rCouncil, const PendingProposal_t& rPending);

} // namespace ac
