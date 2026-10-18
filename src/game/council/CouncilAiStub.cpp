#include "game/council/CouncilAiStub.h"
#include "game/faction/Faction.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilProposalRegistry.h"
#include "game/council/CouncilRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/council/PlanetaryCouncil.h"

namespace ac
{

void CastStubCouncilVotes(CouncilVoteExecutor& rVotes, const PlanetaryCouncil& rCouncil)
{
    const PendingProposal_t* pPending = rVotes.GetPending();
    if (!pPending)
    {
        return;
    }

    const CouncilProposalConfig_t& rConfig = rCouncil.GetRegistry().Get(pPending->proposalId);

    const Faction* pElectionChoice = nullptr;
    if (rConfig.kind == CouncilProposalKind_t::Election)
    {
        // The proposer need not be eligible, and CastElectionVote rejects an ineligible
        // candidate; a throw here would unwind out of Propose's OnProposalOpened emission.
        const std::vector<Faction*> eligible = EligibleCandidates(rCouncil, rConfig);
        for (Faction* pCandidate : eligible)
        {
            if (pCandidate && pCandidate->GetFactionId() == pPending->proposerId)
            {
                pElectionChoice = pCandidate;
                break;
            }
        }
        if (!pElectionChoice && !eligible.empty())
        {
            pElectionChoice = eligible.front();
        }
        // Leaving pElectionChoice null is a valid abstention if nobody is eligible.
    }

    for (Faction* pMember : rCouncil.Members())
    {
        if (!pMember || pMember->IsPlayerControlled())
        {
            continue;
        }

        const FactionId_t id = pMember->GetFactionId();
        if (rConfig.kind == CouncilProposalKind_t::Election)
        {
            if (pPending->electionVotes.find(id) != pPending->electionVotes.end())
            {
                continue;
            }
            rVotes.CastElectionVote(*pMember, pElectionChoice);
        }
        else
        {
            if (pPending->ballots.find(id) != pPending->ballots.end())
            {
                continue;
            }
            rVotes.CastVote(*pMember, CouncilBallot_t::Yea);
        }
    }
}

} // namespace ac
