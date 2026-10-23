#include "game/council/CouncilAiStub.h"
#include "game/faction/Faction.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"

namespace ac
{

namespace
{

// The proposer if eligible, else the first eligible candidate; null abstains when nobody is.
// CastElectionVote rejects an ineligible candidate, and a throw here would unwind out of
// Propose's OnProposalOpened emission.
const Faction* StubElectionChoice_(const PlanetaryCouncil& rCouncil,
                                   const CouncilProposalConfig_t& rConfig,
                                   FactionId_t proposerId)
{
    const std::vector<Faction*> eligible = EligibleCandidates(rCouncil, rConfig);
    for (Faction* pCandidate : eligible)
    {
        if (pCandidate && pCandidate->GetFactionId() == proposerId)
        {
            return pCandidate;
        }
    }
    return eligible.empty() ? nullptr : eligible.front();
}

bool HasVoted_(const PendingProposal_t& rPending, const CouncilProposalConfig_t& rConfig,
               FactionId_t voterId)
{
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        return rPending.electionVotes.contains(voterId);
    }
    return rPending.ballots.contains(voterId);
}

} // namespace

void CastStubCouncilVotes(CouncilVoteExecutor& rVotes, const PlanetaryCouncil& rCouncil)
{
    const PendingProposal_t* pPending = rVotes.GetPending();
    if (!pPending)
    {
        return;
    }
    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(pPending->proposalId);
    const bool bElection = rConfig.kind == CouncilProposalKind_t::Election;
    const Faction* pElectionChoice =
        bElection ? StubElectionChoice_(rCouncil, rConfig, pPending->proposerId) : nullptr;
    for (Faction* pMember : rCouncil.Members())
    {
        if (!pMember || pMember->IsPlayerControlled()
            || HasVoted_(*pPending, rConfig, pMember->GetFactionId()))
        {
            continue;
        }
        if (bElection)
        {
            rVotes.CastElectionVote(*pMember, pElectionChoice);
        }
        else
        {
            rVotes.CastVote(*pMember, CouncilBallot_t::Yea);
        }
    }
}

} // namespace ac
